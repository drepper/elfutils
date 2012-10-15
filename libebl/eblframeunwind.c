/* Get previous frame state for an existing frame state.
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

bool
ebl_frame_unwind (Ebl *ebl, struct Dwfl_Frame_State **statep, Dwarf_Addr pc,
		  bool (*memory_read) (struct Dwfl_Frame_State_Process *process,
				       Dwarf_Addr addr, Dwarf_Addr *result))
{
  if (ebl == NULL || ebl->frame_unwind == NULL)
    return false;
  return ebl->frame_unwind (ebl, statep, pc, memory_read);
}
