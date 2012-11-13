/* Get Dwarf Frame state from modules present in DWFL.
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

Dwfl_Frame_State *
dwfl_frame_state_data (Dwfl *dwfl, bool pc_set, Dwarf_Addr pc, unsigned nregs,
		       const uint64_t *regs_set, const Dwarf_Addr *regs,
		       dwfl_frame_memory_read_t *memory_read,
		       void *memory_read_user_data)
{
  Ebl *ebl = NULL;
  for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL; mod = mod->next)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	continue;
      ebl = mod->ebl;
    }
  if (ebl == NULL || nregs > ebl_frame_state_nregs (ebl))
    {
      __libdwfl_seterrno (DWFL_E_LIBEBL_BAD);
      return NULL;
    }
  Dwfl_Frame_State_Process *process;
  process = __libdwfl_process_alloc (dwfl, memory_read, memory_read_user_data);
  if (process == NULL)
    return NULL;
  process->ebl = ebl;
  Dwfl_Frame_State_Thread *thread = __libdwfl_thread_alloc (process, 0);
  if (thread == NULL)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return NULL;
    }
  Dwfl_Frame_State *state = thread->unwound;
  state->pc_state = DWFL_FRAME_STATE_ERROR;
  if (pc_set)
    {
      state->pc = pc;
      state->pc_state = DWFL_FRAME_STATE_PC_SET;
    }
  for (unsigned regno = 0; regno < nregs; regno++)
    if ((regs_set[regno / sizeof (*regs_set) / 8]
	 & (1U << (regno % (sizeof (*regs_set) * 8)))) != 0
        && ! dwfl_frame_state_reg_set (state, regno, regs[regno]))
      {
	__libdwfl_process_free (process);
	__libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
	return NULL;
      }
  if (! ebl_frame_state (state))
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  if (! __libdwfl_state_fetch_pc (state))
    {
      __libdwfl_process_free (process);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_data)
