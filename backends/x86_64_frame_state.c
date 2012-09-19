/* Fetch live process Dwarf_Frame_State from a PID.
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

#define BACKEND x86_64_
#include "libebl_CPU.h"

Dwarf_Frame_State *
x86_64_frame_state (Ebl *ebl __attribute__ ((unused)), pid_t pid)
{
  Dwarf_Frame_State *state;
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  const size_t nregs = 17;
  struct user_regs_struct user_regs;

  if (pid != 0)
    {
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
      if (ptrace (PTRACE_GETREGS, pid, NULL, &user_regs) != 0)
	{
	  ptrace (PTRACE_DETACH, pid, NULL, NULL);
	  return NULL;
	}
    }

  state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->nregs = nregs;
  state->regs_bits = 64;

  if (pid == 0)
    state->regs_set = 0;
  else
    {
      state->regs[0] = user_regs.rax;
      state->regs[1] = user_regs.rdx;
      state->regs[2] = user_regs.rcx;
      state->regs[3] = user_regs.rbx;
      state->regs[4] = user_regs.rsi;
      state->regs[5] = user_regs.rdi;
      state->regs[6] = user_regs.rbp;
      state->regs[7] = user_regs.rsp;
      state->regs[8] = user_regs.r8;
      state->regs[9] = user_regs.r9;
      state->regs[10] = user_regs.r10;
      state->regs[11] = user_regs.r11;
      state->regs[12] = user_regs.r12;
      state->regs[13] = user_regs.r13;
      state->regs[14] = user_regs.r14;
      state->regs[15] = user_regs.r15;
      state->regs[16] = user_regs.rip;
      state->regs_set = (1U << nregs) - 1;
    }

  return state;
}
INTDEF (x86_64_frame_state)
