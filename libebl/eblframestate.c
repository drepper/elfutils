/* Return location expression to find return value given a function type DIE.
   Copyright (C) 2005 Red Hat, Inc.
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

Dwarf_Frame_State *
ebl_frame_state (Ebl *ebl, pid_t pid)
{
  if (ebl == NULL)
    return NULL;
    
  Dwarf_Frame_State *state = ebl->frame_state (ebl, pid);
  assert (state->nregs > 0);
  assert (state->nregs < sizeof (state->regs_set) * 8);
  /* REGS_SET does not have set any bit out of the NREGS range.  */
  assert ((-(((__typeof (state->regs_set)) 1) << state->nregs) & state->regs_set) == 0);
  state->ebl = ebl;
  return state;
}
