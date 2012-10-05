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

#include <libeblP.h>
#include "../libdw/cfi.h"
#include <assert.h>

bool
ebl_frame_state (Dwarf_Frame_State *state)
{
  Dwarf_Frame_State_Process *process = state->thread->process;
  Ebl *ebl = process->ebl;
  assert (! process->thread->tid != ! process->core);
  /* Otherwise caller could not allocate STATE of proper size.  If FRAME_STATE
     is unsupported then FRAME_STATE_NREGS is zero.  */
  assert (ebl->frame_state != NULL);
  return ebl->frame_state (state);
}

size_t
ebl_frame_state_nregs (Ebl *ebl)
{
  return ebl == NULL ? 0 : ebl->frame_state_nregs;
}
