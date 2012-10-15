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

#include <stdlib.h>
#ifdef __x86_64__
# include <sys/user.h>
# include <sys/ptrace.h>
#endif
#include "libdwflP.h"

#define BACKEND x86_64_
#include "libebl_CPU.h"

bool
x86_64_frame_state (Dwfl_Frame_State *state)
{
  if (state->thread->process->core == NULL)
    {
#ifndef __x86_64__
      return false;
#else /* __x86_64__ */
      pid_t tid = state->thread->tid;
      struct user_regs_struct user_regs;
      if (ptrace (PTRACE_GETREGS, tid, NULL, &user_regs) != 0)
	return false;
      dwfl_frame_state_reg_set (state, 0, user_regs.rax);
      dwfl_frame_state_reg_set (state, 1, user_regs.rdx);
      dwfl_frame_state_reg_set (state, 2, user_regs.rcx);
      dwfl_frame_state_reg_set (state, 3, user_regs.rbx);
      dwfl_frame_state_reg_set (state, 4, user_regs.rsi);
      dwfl_frame_state_reg_set (state, 5, user_regs.rdi);
      dwfl_frame_state_reg_set (state, 6, user_regs.rbp);
      dwfl_frame_state_reg_set (state, 7, user_regs.rsp);
      dwfl_frame_state_reg_set (state, 8, user_regs.r8);
      dwfl_frame_state_reg_set (state, 9, user_regs.r9);
      dwfl_frame_state_reg_set (state, 10, user_regs.r10);
      dwfl_frame_state_reg_set (state, 11, user_regs.r11);
      dwfl_frame_state_reg_set (state, 12, user_regs.r12);
      dwfl_frame_state_reg_set (state, 13, user_regs.r13);
      dwfl_frame_state_reg_set (state, 14, user_regs.r14);
      dwfl_frame_state_reg_set (state, 15, user_regs.r15);
      dwfl_frame_state_reg_set (state, 16, user_regs.rip);
#endif /* __x86_64__ */
    }
  return true;
}
