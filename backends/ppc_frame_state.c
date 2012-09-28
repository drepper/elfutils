/* Fetch live process Dwarf_Frame_State from PID.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include "../libdw/cfi.h"
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define BACKEND ppc_
#include "libebl_CPU.h"

#define BUILD_BUG_ON_ZERO(x) (sizeof (char [(x) ? -1 : 1]) - 1)

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

bool
ppc_frame_dwarf_to_regno (Ebl *ebl __attribute__ ((unused)), unsigned *regno)
{
  switch (*regno)
  {
    case 108:
      *regno = 65;
      return true;
    case 0 ... 107:
    case 109 ... (114 - 1) -1:
      return true;
    case 1200 ... 1231:
      *regno = *regno - 1200 + (114 - 1);
      return true;
    default:
      return false;
  }
  abort ();
}

__typeof (ppc_frame_dwarf_to_regno)
     ppc64_frame_dwarf_to_regno
     __attribute__ ((alias ("ppc_frame_dwarf_to_regno")));

static Dwarf_Frame_State *
frame_state (Ebl *ebl, pid_t pid, bool pid_attach, Elf *core, const unsigned regs_bits)
{
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  const size_t nregs = (114 - 1) + 32;
#ifdef __powerpc__
  union
    {
      struct pt_regs r;
      long l[sizeof (struct pt_regs) / sizeof (long) + BUILD_BUG_ON_ZERO (sizeof (struct pt_regs) % sizeof (long))];
    }
  user_regs;
#endif /* __powerpc__ */
  /* Needless initialization for old GCCs.  */
  Dwarf_Addr core_pc = 0;
  bool core_pc_set;

  if (pid_attach)
    {
#ifndef __powerpc__
      abort ();
#else /* __powerpc__ */
      if (ptrace (PTRACE_ATTACH, pid, NULL, NULL) != 0)
	return NULL;
      for (;;)
	{
	  int status;
	  if (waitpid (pid, &status, 0) != pid || !WIFSTOPPED (status))
	    {
	      ptrace (PTRACE_DETACH, pid, NULL, NULL);
	      return NULL;
	    }
	  if (WSTOPSIG (status) == SIGSTOP)
	    break;
	  if (ptrace (PTRACE_CONT, pid, NULL, (void *) (uintptr_t) WSTOPSIG (status)) != 0)
	    {
	      ptrace (PTRACE_DETACH, pid, NULL, NULL);
	      return NULL;
	    }
	}
#endif /* __powerpc__ */
    }
  if (pid)
    {
#ifndef __powerpc__
      abort ();
#else /* __powerpc__ */
      /* PTRACE_GETREGS is EIO on kernel-2.6.18-308.el5.ppc64.  */
      errno = 0;
      for (unsigned regno = 0; regno < sizeof (user_regs) / sizeof (long); regno++)
	{
	  user_regs.l[regno] = ptrace (PTRACE_PEEKUSER, pid, (void *) (uintptr_t) (regno * sizeof (long)), NULL);
	  if (errno != 0)
	    {
	      if (pid_attach)
		ptrace (PTRACE_DETACH, pid, NULL, NULL);
	      return NULL;
	    }
	}
#endif /* __powerpc__ */
    }
  if (core)
    {
      size_t phnum;
      if (elf_getphdrnum (core, &phnum) < 0)
	return NULL;
      for (size_t cnt = 0; cnt < phnum; ++cnt)
	{
	  GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
	  if (phdr == NULL || phdr->p_type != PT_NOTE)
	    continue;
	  Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
	  if (data == NULL)
	    return NULL;
	  size_t offset = 0;
	  GElf_Nhdr nhdr;
	  size_t name_offset;
	  size_t desc_offset;
	  while (offset < data->d_size
		 && (offset = gelf_getnote (data, offset,
					    &nhdr, &name_offset, &desc_offset)) > 0)
	    {
	      if (nhdr.n_type != NT_PRSTATUS)
		continue;
	      const char *reg_desc = data->d_buf + desc_offset + (regs_bits == 32 ? 0xc8 : 0x170);
	      if (reg_desc + regs_bits / 8 > (const char *) data->d_buf + nhdr.n_descsz)
		continue;
	      Dwarf_Addr val;
	      switch (regs_bits)
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
		    val = val64;
		  }
		  break;
		default:
		  abort ();
	      }
	      if (reg_desc == NULL)
		continue;
	      core_pc = val;
	      core_pc_set = true;
	    }
	}
      if (! core_pc_set)
	return NULL;
    }

  Dwarf_Frame_State_Base *base = malloc (sizeof (*base));
  if (base == NULL)
    return NULL;
  base->ebl = ebl;
  base->nregs = nregs;
  base->regs_bits = regs_bits;
  Dwarf_Frame_State *state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    {
      free (base);
      return NULL;
    }
  base->unwound = state;
  state->base = base;
  state->unwound = NULL;
  state->pc_state = DWARF_FRAME_STATE_ERROR;

  memset (state->regs_set, 0, sizeof (state->regs_set));
  if (pid)
    {
#ifndef __powerpc__
      abort ();
#else /* __powerpc__ */
      for (unsigned gpr = 0; gpr < sizeof (user_regs.r.gpr) / sizeof (*user_regs.r.gpr); gpr++)
	dwarf_frame_state_reg_set (state, gpr, user_regs.r.gpr[gpr]);
      state->pc = user_regs.r.nip;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
      dwarf_frame_state_reg_set (state, 65, user_regs.r.link); // or 108

      /* These registers are probably irrelevant for CFI.  */
      dwarf_frame_state_reg_set (state, 66, user_regs.r.msr);
      dwarf_frame_state_reg_set (state, 109, user_regs.r.ctr);
      dwarf_frame_state_reg_set (state, 101, user_regs.r.xer);
      dwarf_frame_state_reg_set (state, 119, user_regs.r.dar);
      dwarf_frame_state_reg_set (state, 118, user_regs.r.dsisr);
#endif /* __powerpc__ */
    }
  if (core)
    {
      state->pc = core_pc;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
    }

  return state;
}

Dwarf_Frame_State *
ppc_frame_state (Ebl *ebl, pid_t pid, bool pid_attach, Elf *core)
{
  return frame_state (ebl, pid, pid_attach, core, 32);
}

Dwarf_Frame_State *
ppc64_frame_state (Ebl *ebl, pid_t pid, bool pid_attach, Elf *core)
{
  return frame_state (ebl, pid, pid_attach, core, 64);
}

void
ppc_frame_detach (Ebl *ebl __attribute__ ((unused)), pid_t pid)
{
  ptrace (PTRACE_DETACH, pid, NULL, NULL);
}

__typeof (ppc_frame_detach)
     ppc64_frame_detach
     __attribute__ ((alias ("ppc_frame_detach")));
