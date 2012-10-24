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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include "system.h"

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Exact copy from libdwfl/segment.c.  */

static GElf_Addr
segment_start (Dwfl *dwfl, GElf_Addr start)
{
  if (dwfl->segment_align > 1)
    start &= -dwfl->segment_align;
  return start;
}

/* Exact copy from libdwfl/segment.c.  */

static GElf_Addr
segment_end (Dwfl *dwfl, GElf_Addr end)
{
  if (dwfl->segment_align > 1)
    end = (end + dwfl->segment_align - 1) & -dwfl->segment_align;
  return end;
}

static bool
tid_is_attached (Dwfl *dwfl, pid_t tid)
{
  for (Dwfl_Frame_State_Process *process = dwfl->framestatelist; process;
       process = process->next)
    for (Dwfl_Frame_State_Thread *thread = process->thread; thread;
         thread = thread->next)
      if (thread->tid_attached && thread->tid == tid)
	return true;
  return false;
}

static bool
state_fetch_pc (Dwfl_Frame_State *state)
{
  switch (state->pc_state)
  {
    case DWFL_FRAME_STATE_PC_SET:
      return true;
    case DWFL_FRAME_STATE_PC_UNDEFINED:
      abort ();
    case DWFL_FRAME_STATE_ERROR:;
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
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
      return true;
    }
  abort ();
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static void
state_free (Dwfl_Frame_State *state)
{
  Dwfl_Frame_State_Thread *thread = state->thread;
  assert (thread->unwound == state);
  thread->unwound = state->unwound;
  free (state);
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static Dwfl_Frame_State *
state_alloc (Dwfl_Frame_State_Thread *thread)
{
  assert (thread->unwound == NULL);
  Ebl *ebl = thread->process->ebl;
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    return NULL;
  assert (nregs < sizeof (((Dwfl_Frame_State *) NULL)->regs_set) * 8);
  Dwfl_Frame_State *state = malloc (sizeof (*state)
				     + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->thread = thread;
  state->signal_frame = false;
  state->pc_state = DWFL_FRAME_STATE_ERROR;
  memset (state->regs_set, 0, sizeof (state->regs_set));
  thread->unwound = state;
  state->unwound = NULL;
  return state;
}

static void
thread_free (Dwfl_Frame_State_Thread *thread)
{
  while (thread->unwound)
    state_free (thread->unwound);
  if (thread->tid_attached)
    ptrace (PTRACE_DETACH, thread->tid, NULL, NULL);
  Dwfl_Frame_State_Process *process = thread->process;
  assert (process->thread == thread);
  process->thread = thread->next;
  free (thread);
}

/* One state_alloc is called automatically.  */

static Dwfl_Frame_State_Thread *
thread_alloc (Dwfl_Frame_State_Process *process, pid_t tid)
{
  Dwfl_Frame_State_Thread *thread = malloc (sizeof (*thread));
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
process_free (Dwfl_Frame_State_Process *process)
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

static Dwfl_Frame_State_Process *
process_alloc (Dwfl *dwfl, dwfl_frame_memory_read_t *memory_read,
	       void *memory_read_user_data)
{
  Dwfl_Frame_State_Process *process = malloc (sizeof (*process));
  if (process == NULL)
    return NULL;
  process->dwfl = dwfl;
  process->ebl = NULL;
  process->ebl_close = NULL;
  process->memory_read = memory_read;
  process->memory_read_user_data = memory_read_user_data;
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
      if (ptrace (PTRACE_CONT, tid, NULL,
		  (void *) (uintptr_t) WSTOPSIG (status)) != 0)
	{
	  ptrace (PTRACE_DETACH, tid, NULL, NULL);
	  return false;
	}
    }
  return true;
}

static bool
dwfl_frame_state_pid_memory_read (Dwarf_Addr addr, Dwarf_Addr *result,
				  void *user_data)
{
  Dwfl_Frame_State_Process *process = user_data;
  assert (process->core == NULL && process->thread->tid);
  if (process->ebl->class == ELFCLASS64)
    {
      errno = 0;
      *result = ptrace (PTRACE_PEEKDATA, process->thread->tid,
			(void *) (uintptr_t) addr, NULL);
      if (errno != 0)
	{
	  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	  return false;
	}
      return true;
    }
#if SIZEOF_LONG == 8
  /* We do not care about reads unaliged to 4 bytes boundary.
     But 0x...ffc read of 8 bytes could overrun a page.  */
  bool lowered = (addr & 4) != 0;
  if (lowered)
    addr -= 4;
#endif /* SIZEOF_LONG == 8 */
  errno = 0;
  *result = ptrace (PTRACE_PEEKDATA, process->thread->tid,
		    (void *) (uintptr_t) addr, NULL);
  if (errno != 0)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
#if SIZEOF_LONG == 8
# if BYTE_ORDER == BIG_ENDIAN
  if (! lowered)
    *result >>= 32;
# else
  if (lowered)
    *result >>= 32;
# endif
#endif /* SIZEOF_LONG == 8 */
  *result &= 0xffffffff;
  return true;
}

Dwfl_Frame_State *
dwfl_frame_state_pid (Dwfl *dwfl, pid_t pid)
{
  char dirname[64];
  int i = snprintf (dirname, sizeof (dirname), "/proc/%ld/task", (long) pid);
  assert (i > 0 && i < (ssize_t) sizeof (dirname) - 1);
  Dwfl_Frame_State_Process *process;
  process = process_alloc (dwfl, dwfl_frame_state_pid_memory_read, NULL);
  if (process == NULL)
    return NULL;
  process->memory_read_user_data = process;
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
      if (strcmp (dirent->d_name, ".") == 0
	  || strcmp (dirent->d_name, "..") == 0)
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
      Dwfl_Frame_State_Thread *thread = thread_alloc (process, tid);
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
      Dwfl_Frame_State *state = thread->unwound;
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

static bool
dwfl_frame_state_core_memory_read (Dwarf_Addr addr, Dwarf_Addr *result,
				   void *user_data)
{
  Dwfl_Frame_State_Process *process = user_data;
  Elf *core = process->core;
  assert (core != NULL);
  Dwfl *dwfl = process->dwfl;
  static size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return false;
    }
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_LOAD)
	continue;
      /* Bias is zero here, a core file itself has no bias.  */
      GElf_Addr start = segment_start (dwfl, phdr->p_vaddr);
      GElf_Addr end = segment_end (dwfl, phdr->p_vaddr + phdr->p_memsz);
      unsigned bytes = process->ebl->class == ELFCLASS64 ? 8 : 4;
      if (addr < start || addr + bytes > end)
	continue;
      Elf_Data *data;
      data = elf_getdata_rawchunk (core, phdr->p_offset + addr - start,
				   bytes, ELF_T_ADDR);
      if (data == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return false;
	}
      assert (data->d_size == bytes);
      /* FIXME: Currently any arch supported for unwinding supports
	 unaligned access.  */
      if (bytes == 8)
	*result = *(const uint64_t *) data->d_buf;
      else
	*result = *(const uint32_t *) data->d_buf;
      return true;
    }
  __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
  return false;
}

Dwfl_Frame_State *
dwfl_frame_state_core (Dwfl *dwfl, const char *corefile)
{
  Dwfl_Frame_State_Process *process;
  process = process_alloc (dwfl, dwfl_frame_state_core_memory_read, NULL);
  if (process == NULL)
    return NULL;
  process->memory_read_user_data = process;
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
  Dwfl_Frame_State_Thread *thread = NULL;
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset,
					     phdr->p_filesz, ELF_T_NHDR);
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
	      Elf32_Sword val32s;
	      if (item == items + nitems
		  || gelf_convert (core, ELF_T_SWORD, item->type, &val32s,
				   desc + item->offset)
		     == NULL)
		{
		  process_free (process);
		  __libdwfl_seterrno (DWFL_E_BADELF);
		  return NULL;
		}
	      pid_t tid = val32s;
	      eu_static_assert (sizeof val32s <= sizeof tid);
	      if (thread)
		{
		  /* Delay initialization of THREAD till all notes for it have
		     been read in.  */
		  Dwfl_Frame_State *state = thread->unwound;
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
	  Dwfl_Frame_State *state = thread->unwound;
	  desc += regs_offset;
	  for (size_t regloci = 0; regloci < nregloc; regloci++)
	    {
	      const Ebl_Register_Location *regloc = reglocs + regloci;
	      if (regloc->regno >= nregs)
		continue;
	      assert (regloc->bits == 32 || regloc->bits == 64);
	      const char *reg_desc = desc + regloc->offset;
	      for (unsigned regno = regloc->regno;
		   regno < MIN (regloc->regno + (regloc->count ?: 1U), nregs);
		   regno++)
		{
		  /* PPC provides DWARF register 65 irrelevant for
		     CFI which clashes with register 108 (LR) we need.
		     LR (108) is provided earlier (in NT_PRSTATUS) than the # 65.
		     FIXME: It depends now on their order in core notes.  */
		  if (dwfl_frame_state_reg_get (state, regno, NULL))
		    continue;
		  Dwarf_Addr val;
		  switch (regloc->bits)
		  {
		    case 32:;
		      Elf32_Word val32;
		      reg_desc = gelf_convert (core, ELF_T_WORD, ELF_T_WORD,
					       &val32, reg_desc);
		      /* NULL REG_DESC is caught below.  */
		      /* Do a host width conversion.  */
		      val = val32;
		      break;
		    case 64:;
		      Elf64_Xword val64;
		      reg_desc = gelf_convert (core, ELF_T_XWORD, ELF_T_XWORD,
					       &val64, reg_desc);
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
		  /* Registers not valid for CFI are just ignored.  */
		  dwfl_frame_state_reg_set (state, regno, val);
		  reg_desc += regloc->pad;
		}
	    }
	}
    }
  if (thread)
    {
      /* Delay initialization of THREAD till all notes for it have been read
	 in.  */
      Dwfl_Frame_State *state = thread->unwound;
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

Dwfl_Frame_State *
dwfl_frame_state_data (Dwfl *dwfl, bool pc_set, Dwarf_Addr pc, unsigned nregs,
		       const uint64_t *regs_set, const Dwarf_Addr *regs,
		       dwfl_frame_memory_read_t *memory_read,
		       void *memory_read_user_data)
{
  Ebl *ebl = NULL;
  for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL; mod = mod->next)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	continue;
      ebl = mod->ebl;
    }
  if (ebl == NULL || nregs > ebl_frame_state_nregs (ebl))
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  Dwfl_Frame_State_Process *process;
  process = process_alloc (dwfl, memory_read, memory_read_user_data);
  if (process == NULL)
    return NULL;
  process->ebl = ebl;
  Dwfl_Frame_State_Thread *thread = thread_alloc (process, 0);
  if (thread == NULL)
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  Dwfl_Frame_State *state = thread->unwound;
  state->pc_state = DWFL_FRAME_STATE_ERROR;
  if (pc_set)
    {
      state->pc = pc;
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
    }
  for (unsigned regno = 0; regno < nregs; regno++)
    if ((regs_set[regno / sizeof (*regs_set) / 8]
	 & (1U << (regno % (sizeof (*regs_set) * 8)))) != 0
        && ! dwfl_frame_state_reg_set (state, regno, regs[regno]))
      {
	process_free (process);
	__libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	return NULL;
      }
  if (! ebl_frame_state (state) || ! state_fetch_pc (state))
    {
      process_free (process);
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_data)

Dwfl_Frame_State *
dwfl_frame_thread_next (Dwfl_Frame_State *state)
{
  Dwfl_Frame_State_Thread *thread_next = state->thread->next;
  return thread_next ? thread_next->unwound : NULL;
}
INTDEF (dwfl_frame_thread_next)

pid_t
dwfl_frame_tid_get (Dwfl_Frame_State *state)
{
  return state->thread->tid;
}
INTDEF (dwfl_frame_tid_get)
