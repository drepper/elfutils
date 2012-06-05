/* Enumerate the PC ranges covered by a location list.
   Copyright (C) 2010 Red Hat, Inc.

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

#include "relocate.h"
#include <dwarf.h>
#include <assert.h>


ptrdiff_t
dwarf_getlocation_relocatable (Dwarf_Attribute *attr, ptrdiff_t offset,
			       Dwarf_Relocatable *basep,
			       Dwarf_Relocatable *startp,
			       Dwarf_Relocatable *endp,
			       Dwarf_Op **expr, size_t *exprlen)
{
  if (attr == NULL)
    return -1;

  unsigned int sec_idx = IDX_debug_loc;
  Dwarf_Block block;
  if (offset == 0)
    switch (attr->form)
      {
      case DW_FORM_block:
      case DW_FORM_block1:
      case DW_FORM_block2:
      case DW_FORM_block4:
	if (unlikely (attr->cu->version >= 4))
	  {
	    __libdw_seterrno (DWARF_E_NO_LOCLIST);
	    return -1;
	  }

      case DW_FORM_exprloc:
	if (unlikely (INTUSE(dwarf_formblock) (attr, &block) < 0))
	  return -1;

	sec_idx = cu_sec_idx (attr->cu);
	*startp = (Dwarf_Relocatable) { .adjust = 0 };
	*endp = (Dwarf_Relocatable) { .adjust = (Dwarf_Addr) -1 };

	/* A offset into .debug_loc will never be 1, it must be at least a
	   multiple of 4.  So we can return 1 as a special case value to
	   mark there are no ranges to look for on the next call.  */
	offset = 1;
	break;
      }
  else if (offset == 1)
    return 0;

  if (offset != 1)
    /* Iterate from last position.  */
    offset = __libdw_ranges_relocatable (attr->cu, attr, offset,
					 basep, startp, endp, &block);

  /* Parse the block into internal form.  */
  if (offset > 0 && expr != NULL
      && __libdw_getlocation (attr, &block, true, expr, exprlen, sec_idx) < 0)
    offset = -1;

  return offset;
}
