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

#ifdef __s390__
# include <sys/user.h>
# include <asm/ptrace.h>
# include <sys/ptrace.h>
#endif

#define BACKEND s390_
#include "libebl_CPU.h"
/* Must be included after "libebl_CPU.h" for ebl_frame_state_nregs.  */
#include "../libdw/cfi.h"

#define BUILD_BUG_ON_ZERO(x) (sizeof (char [(x) ? -1 : 1]) - 1)

#include "core-get-pc.c"

bool
s390_frame_state (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Base *base = state->base;
  Ebl *ebl = base->ebl;
  pid_t pid = base->pid;
  Elf *core = base->core;
  if (pid)
    {
#ifndef __s390__
      return false;
#else /* __s390__ */
      struct user user_regs;
      ptrace_area parea;
      parea.process_addr = (uintptr_t) &user_regs;
      parea.kernel_addr = 0;
      parea.len = sizeof (user_regs);
      if (ptrace (PTRACE_PEEKUSR_AREA, pid, &parea, NULL) != 0)
	return false;
      /* If we run as s390x we get the 64-bit registers of PID.
         But -m31 executable seems to use only the 32-bit parts of its registers.  */
      for (unsigned u = 0; u < 16; u++)
	dwarf_frame_state_reg_set (state, 0 + u, user_regs.regs.gprs[u]);
      /* Avoid a conversion double -> integer.  */
      for (unsigned u = 0 + BUILD_BUG_ON_ZERO (sizeof (*user_regs.regs.fp_regs.fprs) - sizeof (*state->regs));
	   u < 16; u++)
	dwarf_frame_state_reg_set (state, 16 + u, *(const __typeof (*state->regs) *) &user_regs.regs.fp_regs.fprs[u]);
      state->pc = user_regs.regs.psw.addr;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
#endif /* __s390__ */
    }
  if (core)
    {
      /* Fetch PSWA.  */
      if (! core_get_pc (core, &state->pc, ebl->class == ELFCLASS32 ? 0x4c : 0x78))
	return false;
      state->pc_state = DWARF_FRAME_STATE_PC_SET;
    }
  return true;
}

__typeof (s390_frame_state)
     s390x_frame_state
     __attribute__ ((alias ("s390_frame_state")));

void
s390_normalize_pc (Ebl *ebl __attribute__ ((unused)), Dwarf_Addr *pc)
{
  /* Clear S390 bit 31.  */
  *pc &= (1U << 31) - 1;
}
