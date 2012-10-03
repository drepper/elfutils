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

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Exact copy from src/readelf.c.  */

static const void *
convert (Elf *core, Elf_Type type, uint_fast16_t count,
	 void *value, const void *data, size_t size)
{
  Elf_Data valuedata =
    {
      .d_type = type,
      .d_buf = value,
      .d_size = size ?: gelf_fsize (core, type, count, EV_CURRENT),
      .d_version = EV_CURRENT,
    };
  Elf_Data indata =
    {
      .d_type = type,
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
pid_is_attached (Dwfl *dwfl, pid_t pid)
{
  for (Dwarf_Frame_State_Base *base = dwfl->statebaselist; base; base = base->next)
    if (base->pid_attached && base->pid == pid)
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
      Ebl *ebl = state->base->ebl;
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

static Dwarf_Frame_State_Base *
base_alloc (Dwfl *dwfl, pid_t pid, Elf *core)
{
  Dwarf_Frame_State_Base *base = malloc (sizeof (*base));
  if (base == NULL)
    return NULL;
  base->dwfl = dwfl;
  base->ebl = NULL;
  base->core = core;
  base->core_fd = -1;
  base->pid = pid;
  base->pid_attached = false;
  base->unwound = NULL;
  return base;
}

static void
base_free (Dwarf_Frame_State_Base *base)
{
  assert (base->unwound == NULL);
  assert (base->ebl == NULL);
  if (base->pid_attached)
    ptrace (PTRACE_DETACH, base->pid, NULL, NULL);
  free (base);
}

static bool
ptrace_attach (Dwarf_Frame_State_Base *base)
{
  assert (! base->pid_attached);
  pid_t pid = base->pid;
  if (ptrace (PTRACE_ATTACH, pid, NULL, NULL) != 0)
    return false;
  /* FIXME: Handle missing SIGSTOP on old Linux kernels.  */
  for (;;)
    {
      int status;
      if (waitpid (pid, &status, 0) != pid || !WIFSTOPPED (status))
	{
	  ptrace (PTRACE_DETACH, pid, NULL, NULL);
	  return false;
	}
      if (WSTOPSIG (status) == SIGSTOP)
	break;
      if (ptrace (PTRACE_CONT, pid, NULL, (void *) (uintptr_t) WSTOPSIG (status)) != 0)
	{
	  ptrace (PTRACE_DETACH, pid, NULL, NULL);
	  return false;
	}
    }
  base->pid_attached = true;
  return true;
}

static Dwarf_Frame_State *
state_alloc (Dwarf_Frame_State_Base *base, Ebl *ebl)
{
  assert (base->ebl == NULL);
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    return NULL;
  assert (nregs < sizeof (((Dwarf_Frame_State *) NULL)->regs_set) * 8);
  Dwarf_Frame_State *state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->base = base;
  base->unwound = state;
  base->ebl = ebl;
  state->unwound = NULL;
  state->signal_frame = false;
  state->pc_state = DWARF_FRAME_STATE_ERROR;
  memset (state->regs_set, 0, sizeof (state->regs_set));
  return state;
}

static void
state_free (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Base *base = state->base;
  base->unwound = NULL;
  base->ebl = NULL;
  free (state);
}

/* Allocate STATE with proper backend-dependent size.  Possibly also fetch
   inferior registers if PID is not zero.  */

static Dwarf_Frame_State *
dwfl_frame_state (Dwfl *dwfl, pid_t pid, Elf *core)
{
  if (dwfl == NULL)
    return NULL;
  assert (!pid != !core);
  Dwarf_Frame_State_Base *base = base_alloc (dwfl, pid, core);
  bool pid_attach = pid ? ! pid_is_attached (dwfl, pid) : false;
  {
    Dwfl_Module *mod;
    for (mod = dwfl->modulelist; mod != NULL; mod = mod->next)
      {
	Dwfl_Error error = __libdwfl_module_getebl (mod);
	if (error != DWFL_E_NOERROR)
	  {
	    __libdwfl_seterrno (error);
	    continue;
	  }
	Dwarf_Frame_State *state = state_alloc (base, mod->ebl);
	if (state == NULL)
	  continue;
	if (pid_attach && ! base->pid_attached && ! ptrace_attach (base))
	  continue;
	if (ebl_frame_state (state))
	  {
	    base->next = dwfl->statebaselist;
	    dwfl->statebaselist = base;
	    return state;
	  }
	state_free (state);
      }
  }
  base_free (base);
  __libdwfl_seterrno (DWFL_E_BADELF);
  return NULL;
}

Dwarf_Frame_State *
dwfl_frame_state_pid (Dwfl *dwfl, pid_t pid)
{
  Dwarf_Frame_State *state = dwfl_frame_state (dwfl, pid, NULL);
  assert (state->base->pid == pid);
  if (! state_fetch_pc (state))
    return NULL;
  return state;
}
INTDEF (dwfl_frame_state_pid)

Dwarf_Frame_State *
dwfl_frame_state_core (Dwfl *dwfl, const char *corefile)
{
  /* Fetch inferior registers from a core file.  */
  int core_fd = open64 (corefile, O_RDONLY);
  if (core_fd < 0)
    {
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  Elf *core;
  Dwfl_Error err = __libdw_open_file (&core_fd, &core, true, false);
  if (err != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (err);
      return NULL;
    }
  Dwarf_Frame_State *state = dwfl_frame_state (dwfl, 0, core);
  if (state == NULL)
    {
      elf_end (core);
      close (core_fd);
      return NULL;
    }
  Dwarf_Frame_State_Base *base = state->base;
  assert (base->core == core);
  base->core_fd = core_fd;
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (core, &ehdr_mem);
  assert (ehdr);
  assert (ehdr->e_type == ET_CORE);
  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
    {
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  size_t nregs = ebl_frame_state_nregs (ebl);
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
      if (data == NULL)
	{
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
		  /* PPC provides in DWARF register 65 irrelevant for
		     CFI which clashes with register 108 (LR) we need.
		     FIXME: It depends now on their order in core notes.  */
		  if (regloc->shift == 0 && dwarf_frame_state_reg_get (state, regno, NULL))
		    continue;
		  Dwarf_Addr val;
		  switch (regloc->bits)
		  {
		    case 32:
		      {
			uint32_t val32;
			reg_desc = convert (core, ELF_T_WORD, 1, &val32, reg_desc, 0);
			/* NULL REG_DESC is caught below.  */
			/* Do a host width conversion.  */
			val = val32;
		      }
		      break;
		    case 64:
		      {
			uint64_t val64;
			reg_desc = convert (core, ELF_T_XWORD, 1, &val64, reg_desc, 0);
			/* NULL REG_DESC is caught below.  */
			assert (sizeof (*state->regs) == sizeof (val64));
			val = val64;
		      }
		      break;
		    default:
		      abort ();
		  }
		  if (reg_desc == NULL)
		    {
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
  if (! state_fetch_pc (state))
    return NULL;
  return state;
}
INTDEF (dwfl_frame_state_core)
