/* Fetch process data from STATE->base->pid or STATE->base->core.
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

#if defined __i386__ || defined __x86_64__
# include <sys/user.h>
# include <sys/ptrace.h>
#endif
#include "libdwflP.h"

#define BACKEND i386_
#include "libebl_CPU.h"

bool
i386_frame_state (Dwfl_Frame_State *state)
{
  if (state->thread->process->core == NULL)
    {
#if !defined __i386__ && !defined __x86_64__
      return false;
#else /* __i386__ || __x86_64__ */
      pid_t tid = state->thread->tid;
      struct user_regs_struct user_regs;
      if (ptrace (PTRACE_GETREGS, tid, NULL, &user_regs) != 0)
	return false;
# if defined __i386__
      dwfl_frame_state_reg_set (state, 0, user_regs.eax);
      dwfl_frame_state_reg_set (state, 1, user_regs.ecx);
      dwfl_frame_state_reg_set (state, 2, user_regs.edx);
      dwfl_frame_state_reg_set (state, 3, user_regs.ebx);
      dwfl_frame_state_reg_set (state, 4, user_regs.esp);
      dwfl_frame_state_reg_set (state, 5, user_regs.ebp);
      dwfl_frame_state_reg_set (state, 6, user_regs.esi);
      dwfl_frame_state_reg_set (state, 7, user_regs.edi);
      dwfl_frame_state_reg_set (state, 8, user_regs.eip);
# elif defined __x86_64__
      dwfl_frame_state_reg_set (state, 0, user_regs.rax);
      dwfl_frame_state_reg_set (state, 1, user_regs.rcx);
      dwfl_frame_state_reg_set (state, 2, user_regs.rdx);
      dwfl_frame_state_reg_set (state, 3, user_regs.rbx);
      dwfl_frame_state_reg_set (state, 4, user_regs.rsp);
      dwfl_frame_state_reg_set (state, 5, user_regs.rbp);
      dwfl_frame_state_reg_set (state, 6, user_regs.rsi);
      dwfl_frame_state_reg_set (state, 7, user_regs.rdi);
      dwfl_frame_state_reg_set (state, 8, user_regs.rip);
# else /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
#  error
# endif /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
#endif /* __i386__ || __x86_64__ */
    }
  return true;
}
