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

#ifdef __powerpc__
# include <sys/user.h>
# include <sys/ptrace.h>
#endif
#include "../libdw/cfi.h"

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

bool
ppc_frame_state (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Thread *thread = state->thread;
  Dwarf_Frame_State_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  pid_t tid = thread->tid;
  Elf *core = process->core;
  if (tid)
    {
#ifndef __powerpc__
      return false;
#else /* __powerpc__ */
      union
	{
	  struct pt_regs r;
	  long l[sizeof (struct pt_regs) / sizeof (long) + BUILD_BUG_ON_ZERO (sizeof (struct pt_regs) % sizeof (long))];
	}
      user_regs;
      /* PTRACE_GETREGS is EIO on kernel-2.6.18-308.el5.ppc64.  */
      errno = 0;
      for (unsigned regno = 0; regno < sizeof (user_regs) / sizeof (long); regno++)
	{
	  user_regs.l[regno] = ptrace (PTRACE_PEEKUSER, tid, (void *) (uintptr_t) (regno * sizeof (long)), NULL);
	  if (errno != 0)
	    return false;
	}
      for (unsigned gpr = 0; gpr < sizeof (user_regs.r.gpr) / sizeof (*user_regs.r.gpr); gpr++)
	dwarf_frame_state_reg_set (state, gpr, user_regs.r.gpr[gpr]);
      state->pc = user_regs.r.nip;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
      dwarf_frame_state_reg_set (state, 65, user_regs.r.link); // or 108
      /* Registers like msr, ctr, xer, dar, dsisr etc. are probably irrelevant
	 for CFI.  */
#endif /* __powerpc__ */
    }
  if (core)
    {
      if (! core_get_pc (core, &state->pc, ebl->class == ELFCLASS64 ? 0x170 : 0xc8))
	return false;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
    }
  return true;
}

__typeof (ppc_frame_state)
     ppc64_frame_state
     __attribute__ ((alias ("ppc_frame_state")));
