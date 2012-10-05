/* Get Dwarf Frame state for target PID or core file.
   Copyright (C) 2012 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include "libdwflP.h"
#include "../libdw/cfi.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define BUILD_BUG_ON_ZERO(x) (sizeof (char [(x) ? -1 : 1]) - 1)

/* Modified copy from src/readelf.c.  */

static const void *
convert (Elf *core, Elf_Type valuetype, Elf_Type datatype, uint_fast16_t count,
	 void *value, const void *data, size_t size)
{
  Elf_Data valuedata =
    {
      .d_type = valuetype,
      .d_buf = value,
      .d_size = size ?: gelf_fsize (core, valuetype, count, EV_CURRENT),
      .d_version = EV_CURRENT,
    };
  Elf_Data indata =
    {
      .d_type = datatype,
      .d_buf = (void *) data,
      .d_size = valuedata.d_size,
      .d_version = EV_CURRENT,
    };

  Elf_Data *d = (gelf_getclass (core) == ELFCLASS32
		 ? elf32_xlatetom : elf64_xlatetom)
    (&valuedata, &indata, elf_getident (core, NULL)[EI_DATA]);
  if (d == NULL)
    return NULL;

  return data + indata.d_size;
}

static bool
tid_is_attached (Dwfl *dwfl, pid_t tid)
{
  for (Dwarf_Frame_State_Process *process = dwfl->framestatelist; process; process = process->next)
    for (Dwarf_Frame_State_Thread *thread = process->thread; thread; thread = thread->next)
      if (thread->tid_attached && thread->tid == tid)
	return true;
  return false;
}

static bool
state_fetch_pc (Dwarf_Frame_State *state)
{
  switch (state->pc_state)
  {
    case DWARF_FRAME_STATE_PC_SET:
      return true;
    case DWARF_FRAME_STATE_PC_UNDEFINED:
      abort ();
    case DWARF_FRAME_STATE_ERROR:;
      Ebl *ebl = state->thread->process->ebl;
      Dwarf_CIE abi_info;
      if (ebl_abi_cfi (ebl, &abi_info) != 0)
	{
	  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	  return false;
	}
      unsigned ra = abi_info.return_address_register;
      /* dwarf_frame_state_reg_is_set is not applied here.  */
      if (ra >= ebl_frame_state_nregs (ebl))
	{
	  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	  return false;
	}
      state->pc = state->regs[ra];
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
      return true;
    }
  abort ();
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static void
state_free (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Thread *thread = state->thread;
  assert (thread->unwound == state);
  thread->unwound = state->unwound;
  free (state);
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static Dwarf_Frame_State *
state_alloc (Dwarf_Frame_State_Thread *thread)
{
  assert (thread->unwound == NULL);
  Ebl *ebl = thread->process->ebl;
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    return NULL;
  assert (nregs < sizeof (((Dwarf_Frame_State *) NULL)->regs_set) * 8);
  Dwarf_Frame_State *state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->thread = thread;
  state->signal_frame = false;
  state->pc_state = DWARF_FRAME_STATE_ERROR;
  memset (state->regs_set, 0, sizeof (state->regs_set));
  thread->unwound = state;
  state->unwound = NULL;
  return state;
}

static void
thread_free (Dwarf_Frame_State_Thread *thread)
{
  while (thread->unwound)
    state_free (thread->unwound);
  if (thread->tid_attached)
    ptrace (PTRACE_DETACH, thread->tid, NULL, NULL);
  Dwarf_Frame_State_Process *process = thread->process;
  assert (process->thread == thread);
  process->thread = thread->next;
  free (thread);
}

/* One state_alloc is called automatically.  */

static Dwarf_Frame_State_Thread *
thread_alloc (Dwarf_Frame_State_Process *process, pid_t tid)
{
  Dwarf_Frame_State_Thread *thread = malloc (sizeof (*thread));
  if (thread == NULL)
    return NULL;
  thread->process = process;
  thread->tid = tid;
  thread->tid_attached = false;
  thread->unwound = NULL;
  thread->next = process->thread;
  process->thread = thread;
  if (state_alloc (thread) == NULL)
    {
      thread_free (thread);
      return NULL;
    }
  return thread;
}

static void
process_free (Dwarf_Frame_State_Process *process)
{
  while (process->thread)
    thread_free (process->thread);
  if (process->ebl_close)
    ebl_closebackend (process->ebl);
  elf_end (process->core);
  if (process->core_fd != -1)
    close (process->core_fd);
  Dwfl *dwfl = process->dwfl;
  assert (dwfl->framestatelist == process);
  dwfl->framestatelist = process->next;
  free (process);
}

static Dwarf_Frame_State_Process *
process_alloc (Dwfl *dwfl)
{
  Dwarf_Frame_State_Process *process = malloc (sizeof (*process));
  if (process == NULL)
    return NULL;
  process->dwfl = dwfl;
  process->ebl = NULL;
  process->ebl_close = NULL;
  process->core = NULL;
  process->core_fd = -1;
  process->thread = NULL;
  process->next = dwfl->framestatelist;
  dwfl->framestatelist = process;
  return process;
}

static bool
ptrace_attach (pid_t tid)
{
  if (ptrace (PTRACE_ATTACH, tid, NULL, NULL) != 0)
    return false;
  /* FIXME: Handle missing SIGSTOP on old Linux kernels.  */
  for (;;)
    {
      int status;
      if (waitpid (tid, &status, __WALL) != tid || !WIFSTOPPED (status))
	{
	  ptrace (PTRACE_DETACH, tid, NULL, NULL);
	  return false;
	}
      if (WSTOPSIG (status) == SIGSTOP)
	break;
      if (ptrace (PTRACE_CONT, tid, NULL, (void *) (uintptr_t) WSTOPSIG (status)) != 0)
	{
	  ptrace (PTRACE_DETACH, tid, NULL, NULL);
	  return false;
	}
    }
  return true;
}

Dwarf_Frame_State *
dwfl_frame_state_pid (Dwfl *dwfl, pid_t pid)
{
  char dirname[64];
  int i = snprintf (dirname, sizeof (dirname), "/proc/%ld/task", (long) pid);
  assert (i > 0 && i < (ssize_t) sizeof (dirname) - 1);
  Dwarf_Frame_State_Process *process = process_alloc (dwfl);
  if (process == NULL)
    return NULL;
  for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL; mod = mod->next)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	continue;
      process->ebl = mod->ebl;
    }
  if (process->ebl == NULL)
    {
      /* Not idenified EBL from any of the modules.  */
      process_free (process);
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  DIR *dir = opendir (dirname);
  if (dir == NULL)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_ERRNO);
      return NULL;
    }
  for (;;)
    {
      errno = 0;
      struct dirent *dirent = readdir (dir);
      if (dirent == NULL)
	{
	  if (errno == 0)
	    break;
	  process_free (process);
	  __libdwfl_seterrno (DWFL_E_ERRNO);
	  return NULL;
	}
      if (strcmp (dirent->d_name, ".") == 0 || strcmp (dirent->d_name, "..") == 0)
	continue;
      char *end;
      errno = 0;
      long tidl = strtol (dirent->d_name, &end, 10);
      if (errno != 0)
	{
	  process_free (process);
	  __libdwfl_seterrno (DWFL_E_ERRNO);
	  return NULL;
	}
      pid_t tid = tidl;
      if (tidl <= 0 || (end && *end) || tid != tidl)
	{
	  process_free (process);
	  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	  return NULL;
	}
      Dwarf_Frame_State_Thread *thread = thread_alloc (process, tid);
      if (thread == NULL)
	{
	  process_free (process);
	  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	  return NULL;
	}
      if (! tid_is_attached (dwfl, thread->tid))
	{
	  if (! ptrace_attach (thread->tid))
	    {
	      thread_free (thread);
	      continue;
	    }
	  thread->tid_attached = true;
	}
      Dwarf_Frame_State *state = thread->unwound;
      if (! ebl_frame_state (state) || ! state_fetch_pc (state))
	{
	  thread_free (thread);
	  continue;
	}
    }
  if (closedir (dir) != 0)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_ERRNO);
      return NULL;
    }
  if (process->thread == NULL)
    {
      /* No valid threads recognized.  */
      process_free (process);
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_pid)

Dwarf_Frame_State *
dwfl_frame_state_core (Dwfl *dwfl, const char *corefile)
{
  Dwarf_Frame_State_Process *process = process_alloc (dwfl);
  if (process == NULL)
    return NULL;
  /* Fetch inferior registers from a core file.  */
  int core_fd = open64 (corefile, O_RDONLY);
  if (core_fd < 0)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  process->core_fd = core_fd;
  Elf *core;
  Dwfl_Error err = __libdw_open_file (&core_fd, &core, true, false);
  if (err != DWFL_E_NOERROR)
    {
      process_free (process);
      __libdwfl_seterrno (err);
      return NULL;
    }
  process->core = core;
  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  process->ebl = ebl;
  process->ebl_close = true;
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    {
      /* We do not support unwinding this CORE file EBL.  */
      process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (core, &ehdr_mem);
  if (ehdr == NULL)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  assert (ehdr->e_type == ET_CORE);
  size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  Dwarf_Frame_State_Thread *thread = NULL;
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
      if (data == NULL)
	{
	  process_free (process);
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return NULL;
	}
      size_t offset = 0;
      GElf_Nhdr nhdr;
      size_t name_offset;
      size_t desc_offset;
      while (offset < data->d_size
	     && (offset = gelf_getnote (data, offset,
					&nhdr, &name_offset, &desc_offset)) > 0)
	{
	  /* Do not check NAME for now, help broken Linux kernels.  */
	  const char *name = data->d_buf + name_offset;
	  const char *desc = data->d_buf + desc_offset;
	  GElf_Word regs_offset;
	  size_t nregloc;
	  const Ebl_Register_Location *reglocs;
	  size_t nitems;
	  const Ebl_Core_Item *items;
	  if (! ebl_core_note (ebl, &nhdr, name,
			       &regs_offset, &nregloc, &reglocs, &nitems, &items))
	    {
	      /* This note may be just not recognized, skip it.  */
	      continue;
	    }
	  if (nhdr.n_type == NT_PRSTATUS)
	    {
	      const Ebl_Core_Item *item;
	      for (item = items; item < items + nitems; item++)
		if (strcmp (item->name, "pid") == 0)
		  break;
	      int32_t val32s;
	      if (item == items + nitems
		  || convert (core, ELF_T_SWORD, item->type, 1, &val32s, desc + item->offset, 0) == NULL)
		{
		  process_free (process);
		  __libdwfl_seterrno (DWFL_E_BADELF);
		  return NULL;
		}
	      pid_t tid = val32s + BUILD_BUG_ON_ZERO (sizeof (val32s) <= sizeof (pid_t) ? 0 : -1);
	      if (thread)
		{
		  /* Delay initialization of THREAD till all notes for it have been read in.  */
		  Dwarf_Frame_State *state = thread->unwound;
		  if (! ebl_frame_state (state) || ! state_fetch_pc (state))
		    {
		      thread_free (thread);
		      thread = NULL;
		      continue;
		    }
		}
	      thread = thread_alloc (process, tid);
	      if (thread == NULL)
		{
		  process_free (process);
		  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
		  return NULL;
		}
	    }
	  if (thread == NULL)
	    {
	      /* Ignore notes before first PR_NTSTATUS.  */
	      continue;
	    }
	  Dwarf_Frame_State *state = thread->unwound;
	  desc += regs_offset;
	  for (size_t regloci = 0; regloci < nregloc; regloci++)
	    {
	      const Ebl_Register_Location *regloc = reglocs + regloci;
	      if (regloc->regno >= nregs)
		continue;
	      assert (regloc->bits == 32 || regloc->bits == 64);
	      const char *reg_desc = desc + regloc->offset;
	      for (unsigned regno = regloc->regno; regno < MIN (regloc->regno + (regloc->count ?: 1U), nregs); regno++)
		{
		  /* PPC provides DWARF register 65 irrelevant for
		     CFI which clashes with register 108 (LR) we need.
		     LR (108) is provided earlier (in NT_PRSTATUS) than the # 65.
		     FIXME: It depends now on their order in core notes.  */
		  if (regloc->shift == 0 && dwarf_frame_state_reg_get (state, regno, NULL))
		    continue;
		  Dwarf_Addr val;
		  switch (regloc->bits)
		  {
		    case 32:;
		      Elf32_Word val32;
		      reg_desc = convert (core, ELF_T_WORD, ELF_T_WORD, 1, &val32, reg_desc, 0);
		      /* NULL REG_DESC is caught below.  */
		      /* Do a host width conversion.  */
		      val = val32;
		      break;
		    case 64:;
		      Elf32_Xword val64;
		      reg_desc = convert (core, ELF_T_XWORD, ELF_T_XWORD, 1, &val64, reg_desc, 0);
		      /* NULL REG_DESC is caught below.  */
		      assert (sizeof (*state->regs) == sizeof (val64));
		      val = val64;
		      break;
		    default:
		      abort ();
		  }
		  if (reg_desc == NULL)
		    {
		      process_free (process);
		      __libdwfl_seterrno (DWFL_E_BADELF);
		      return NULL;
		    }
		  if (regloc->shift)
		    {
		      Dwarf_Addr val_low;
		      bool ok = dwarf_frame_state_reg_get (state, regno, &val_low);
		      assert (ok);
		      val = (val << regloc->shift) | val_low;
		    }
		  /* Registers not valid for CFI are just ignored.  */
		  dwarf_frame_state_reg_set (state, regno, val);
		  reg_desc += regloc->pad;
		}
	    }
	}
    }
  if (thread)
    {
      /* Delay initialization of THREAD till all notes for it have been read in.  */
      Dwarf_Frame_State *state = thread->unwound;
      if (! ebl_frame_state (state) || ! state_fetch_pc (state))
	thread_free (thread);
    }
  if (process->thread == NULL)
    {
      /* No valid threads recognized in this CORE.  */
      process_free (process);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_core)

Dwarf_Frame_State *
dwfl_frame_thread_next (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Thread *thread_next = state->thread->next;
  return thread_next ? thread_next->unwound : NULL;
}
INTDEF (dwfl_frame_thread_next)

pid_t
dwfl_frame_tid_get (Dwarf_Frame_State *state)
{
  return state->thread->tid;
}
INTDEF (dwfl_frame_tid_get)
