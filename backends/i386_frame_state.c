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
#include "../libdw/cfi.h"

#define BACKEND i386_
#include "libebl_CPU.h"

bool
i386_frame_state (Dwarf_Frame_State *state)
{
  pid_t pid = state->base->pid;
  if (pid)
    {
#if !defined __i386__ && !defined __x86_64__
      return false;
#else /* __i386__ || __x86_64__ */
      struct user_regs_struct user_regs;
      if (ptrace (PTRACE_GETREGS, pid, NULL, &user_regs) != 0)
	return false;
# if defined __i386__
      dwarf_frame_state_reg_set (state, 0, user_regs.eax);
      dwarf_frame_state_reg_set (state, 1, user_regs.ecx);
      dwarf_frame_state_reg_set (state, 2, user_regs.edx);
      dwarf_frame_state_reg_set (state, 3, user_regs.ebx);
      dwarf_frame_state_reg_set (state, 4, user_regs.esp);
      dwarf_frame_state_reg_set (state, 5, user_regs.ebp);
      dwarf_frame_state_reg_set (state, 6, user_regs.esi);
      dwarf_frame_state_reg_set (state, 7, user_regs.edi);
      dwarf_frame_state_reg_set (state, 8, user_regs.eip);
# elif defined __x86_64__
      dwarf_frame_state_reg_set (state, 0, user_regs.rax);
      dwarf_frame_state_reg_set (state, 1, user_regs.rcx);
      dwarf_frame_state_reg_set (state, 2, user_regs.rdx);
      dwarf_frame_state_reg_set (state, 3, user_regs.rbx);
      dwarf_frame_state_reg_set (state, 4, user_regs.rsp);
      dwarf_frame_state_reg_set (state, 5, user_regs.rbp);
      dwarf_frame_state_reg_set (state, 6, user_regs.rsi);
      dwarf_frame_state_reg_set (state, 7, user_regs.rdi);
      dwarf_frame_state_reg_set (state, 8, user_regs.rip);
# else /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
#  error
# endif /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
#endif /* __i386__ || __x86_64__ */
    }
  return true;
}
