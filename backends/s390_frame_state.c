/* Fetch live process Dwfl_Frame_State from PID.
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
#include "libdwflP.h"

#define BACKEND s390_
#include "libebl_CPU.h"

#include "core-get-pc.c"

bool
s390_frame_state (Dwfl_Frame_State *state)
{
  Dwfl_Frame_State_Thread *thread = state->thread;
  Dwfl_Frame_State_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  Elf *core = process->core;
  pid_t tid = thread->tid;
  if (core == NULL && tid)
    {
#ifndef __s390__
      return false;
#else /* __s390__ */
      struct user user_regs;
      ptrace_area parea;
      parea.process_addr = (uintptr_t) &user_regs;
      parea.kernel_addr = 0;
      parea.len = sizeof (user_regs);
      if (ptrace (PTRACE_PEEKUSR_AREA, tid, &parea, NULL) != 0)
	return false;
      /* If we run as s390x we get the 64-bit registers of tid.
	 But -m31 executable seems to use only the 32-bit parts of its
	 registers so we ignore the upper half.  */
      for (unsigned u = 0; u < 16; u++)
	dwarf_frame_state_reg_set (state, 0 + u, user_regs.regs.gprs[u]);
      /* Avoid conversion double -> integer.  */
      eu_static_assert (sizeof user_regs.regs.fp_regs.fprs[0]
                        == sizeof state->regs[0]);
      for (unsigned u = 0; u < 16; u++)
	dwarf_frame_state_reg_set (state, 16 + u,
				   *((const __typeof (*state->regs) *)
				     &user_regs.regs.fp_regs.fprs[u]));
      state->pc = user_regs.regs.psw.addr;
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
#endif /* __s390__ */
    }
  else if (core)
    {
      /* Fetch PSWA.  */
      if (! core_get_pc (core, &state->pc,
			 ebl->class == ELFCLASS32 ? 0x4c : 0x78))
	return false;
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
    }
  return true;
}

void
s390_normalize_pc (Ebl *ebl __attribute__ ((unused)), Dwarf_Addr *pc)
{
  assert (ebl->class == ELFCLASS32);

  /* Clear S390 bit 31.  */
  *pc &= (1U << 31) - 1;
}
