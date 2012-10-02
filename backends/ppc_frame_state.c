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

#include "core-get-pc.c"

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
      core_pc_set = core_get_pc (core, &core_pc, regs_bits == 32 ? 0xc8 : 0x170);
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
