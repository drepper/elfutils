/* Enumerate the PC ranges covered by a location list.
   Copyright (C) 2010 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

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
