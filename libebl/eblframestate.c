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

Dwarf_Frame_State *
ebl_frame_state (Ebl *ebl, pid_t pid, bool pid_attach)
{
  if (ebl == NULL)
    return NULL;
    
  assert (!pid_attach || pid);
  Dwarf_Frame_State *state = ebl->frame_state (ebl, pid, pid_attach);
  if (state == NULL)
    return NULL;
  Dwarf_Frame_State_Base *base = state->base;
  base->ebl = ebl;
  base->core = NULL;
  base->core_fd = -1;
  base->pid = pid;
  base->pid_attached = pid_attach;
  base->unwound = state;
  assert (base->nregs > 0);
  assert (base->nregs < sizeof (state->regs_set) * 8);
  /* REGS_SET does not have set any bit out of the NREGS range.  */
  assert ((-(((__typeof (state->regs_set)) 1) << base->nregs) & state->regs_set) == 0);
  return state;
}
INTDEF(ebl_frame_state)
