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
#include <assert.h>

#define BACKEND i386_
#include "libebl_CPU.h"

Dwarf_Frame_State *
i386_frame_state (Ebl *ebl __attribute__ ((unused)), pid_t pid, bool pid_attach)
{
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  For i386 it is 17, why?  */
  const size_t nregs = 9;
#ifdef __i386__
  struct user_regs_struct user_regs;
#endif /* __i386__ */

  if (pid_attach)
    {
#ifndef __i386__
      abort ();
#else /* __i386__ */
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
#endif /* __i386__ */
    }
  if (pid)
    {
#ifndef __i386__
      abort ();
#else /* __i386__ */
      if (ptrace (PTRACE_GETREGS, pid, NULL, &user_regs) != 0)
	{
	  if (pid_attach)
	    ptrace (PTRACE_DETACH, pid, NULL, NULL);
	  return NULL;
	}
#endif /* __i386__ */
    }

  Dwarf_Frame_State_Base *base = malloc (sizeof (*base));
  if (base == NULL)
    return NULL;
  base->nregs = nregs;
  base->regs_bits = 32;
  Dwarf_Frame_State *state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    {
      free (base);
      return NULL;
    }
  base->unwound = state;
  state->base = base;
  state->unwound = NULL;

  if (pid == 0)
    state->regs_set = 0;
  else
    {
#ifndef __i386__
      abort ();
#else /* __i386__ */
      state->regs[0] = user_regs.eax;
      state->regs[1] = user_regs.ecx;
      state->regs[2] = user_regs.edx;
      state->regs[3] = user_regs.ebx;
      state->regs[4] = user_regs.esp;
      state->regs[5] = user_regs.ebp;
      state->regs[6] = user_regs.esi;
      state->regs[7] = user_regs.edi;
      state->regs[8] = user_regs.eip;
      state->regs_set = (1U << nregs) - 1;
#endif /* __i386__ */
    }

  return state;
}
INTDEF (i386_frame_state)
