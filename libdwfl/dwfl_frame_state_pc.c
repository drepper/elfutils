/* Get return address register value for frame.
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

#include "../libdw/cfi.h"
#include "../libebl/libebl.h"
#include "libdwflP.h"

bool
dwfl_frame_state_pc (Dwarf_Frame_State *state, Dwarf_Addr *pc, bool *minusone)
{
  Dwarf_CIE abi_info;
  unsigned ra;

  if (ebl_abi_cfi (state->base->ebl, &abi_info) != 0)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
  ra = abi_info.return_address_register;
  if (ra >= state->base->nregs)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
  if ((state->regs_set & (1U << ra)) == 0)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
  *pc = state->regs[ra];
  if (minusone)
    {
      /* Bottom frame?  */
      if (state == state->base->unwound)
	*minusone = false;
      /* *MINUSONE is logical or of both current and previous frame state.  */
      else if (state->signal_frame)
	*minusone = false;
      /* Not affected by unsuccessfully unwound frame.  */
      else if (! dwfl_frame_unwind (&state) || state == NULL)
	*minusone = true;
      else
	*minusone = ! state->signal_frame;
    }
  return true;
}
INTDEF (dwfl_frame_state_pc)
