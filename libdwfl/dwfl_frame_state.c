/* Get Dwarf Frame state for target PID or core file.
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

#include "libdwflP.h"
#include <sys/ptrace.h>
#include <unistd.h>

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

bool
internal_function
__libdwfl_state_fetch_pc (Dwfl_Frame_State *state)
{
  switch (state->pc_state)
  {
    case DWFL_FRAME_STATE_PC_SET:
      return true;
    case DWFL_FRAME_STATE_PC_UNDEFINED:
      abort ();
    case DWFL_FRAME_STATE_ERROR:;
      Ebl *ebl = state->thread->process->ebl;
      Dwarf_CIE abi_info;
      if (ebl_abi_cfi (ebl, &abi_info) != 0)
	{
	  __libdwfl_seterrno (DWFL_E_LIBEBL);
	  return false;
	}
      unsigned ra = abi_info.return_address_register;
      /* dwarf_frame_state_reg_is_set is not applied here.  */
      if (ra >= ebl_frame_state_nregs (ebl))
	{
	  __libdwfl_seterrno (DWFL_E_LIBEBL_BAD);
	  return false;
	}
      state->pc = state->regs[ra];
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
      return true;
    }
  abort ();
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static void
state_free (Dwfl_Frame_State *state)
{
  Dwfl_Frame_State_Thread *thread = state->thread;
  assert (thread->unwound == state);
  thread->unwound = state->unwound;
  free (state);
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static Dwfl_Frame_State *
state_alloc (Dwfl_Frame_State_Thread *thread)
{
  assert (thread->unwound == NULL);
  Ebl *ebl = thread->process->ebl;
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    return NULL;
  assert (nregs < sizeof (((Dwfl_Frame_State *) NULL)->regs_set) * 8);
  Dwfl_Frame_State *state = malloc (sizeof (*state)
				     + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->thread = thread;
  state->signal_frame = false;
  state->pc_state = DWFL_FRAME_STATE_ERROR;
  memset (state->regs_set, 0, sizeof (state->regs_set));
  thread->unwound = state;
  state->unwound = NULL;
  return state;
}

void
internal_function
__libdwfl_thread_free (Dwfl_Frame_State_Thread *thread)
{
  while (thread->unwound)
    state_free (thread->unwound);
  if (thread->tid_attached)
    ptrace (PTRACE_DETACH, thread->tid, NULL, NULL);
  Dwfl_Frame_State_Process *process = thread->process;
  assert (process->thread == thread);
  process->thread = thread->next;
  free (thread);
}

Dwfl_Frame_State_Thread *
internal_function
__libdwfl_thread_alloc (Dwfl_Frame_State_Process *process, pid_t tid)
{
  Dwfl_Frame_State_Thread *thread = malloc (sizeof (*thread));
  if (thread == NULL)
    return NULL;
  thread->process = process;
  thread->tid = tid;
  thread->tid_attached = false;
  thread->unwound = NULL;
  thread->next = process->thread;
  process->thread = thread;
  if (state_alloc (thread) == NULL)
    {
      __libdwfl_thread_free (thread);
      return NULL;
    }
  return thread;
}

void
internal_function
__libdwfl_process_free (Dwfl_Frame_State_Process *process)
{
  while (process->thread)
    __libdwfl_thread_free (process->thread);
  if (process->ebl_close)
    ebl_closebackend (process->ebl);
  elf_end (process->core);
  if (process->core_fd != -1)
    close (process->core_fd);
  Dwfl *dwfl = process->dwfl;
  assert (dwfl->framestatelist == process);
  dwfl->framestatelist = process->next;
  free (process);
}

Dwfl_Frame_State_Process *
internal_function
__libdwfl_process_alloc (Dwfl *dwfl, dwfl_frame_memory_read_t *memory_read,
			 void *memory_read_user_data)
{
  Dwfl_Frame_State_Process *process = malloc (sizeof (*process));
  if (process == NULL)
    return NULL;
  process->dwfl = dwfl;
  process->ebl = NULL;
  process->ebl_close = NULL;
  process->memory_read = memory_read;
  process->memory_read_user_data = memory_read_user_data;
  process->core = NULL;
  process->core_fd = -1;
  process->thread = NULL;
  process->next = dwfl->framestatelist;
  dwfl->framestatelist = process;
  return process;
}
