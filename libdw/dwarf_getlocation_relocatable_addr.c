/* Return relocatable address from DW_OP_addr in a DWARF expression.
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

#include "libdwP.h"
#include <dwarf.h>

int
dwarf_getlocation_relocatable_addr (attr, op, reloc)
     Dwarf_Attribute *attr;
     const Dwarf_Op *op;
     Dwarf_Relocatable *reloc;
{
  if (attr == NULL)
    return -1;

  if (unlikely (op->atom != DW_OP_addr))
    {
      __libdw_seterrno (DWARF_E_INVALID_ACCESS);
      return -1;
    }

  *reloc = (Dwarf_Relocatable)
    {
      .form = DW_FORM_addr,
      .cu = attr->cu,
      .adjust = op->number,
      .symndx = op->number2,
    };

  if (reloc->symndx != 0)
    /* Setting .valp non-null with .symndx nonzero indicates that
       the adjustment is relative to a known symndx, but not resolved
       to section-relative.  */
    reloc->valp = (void *) -1l;

  /* If the attribute this expression came from was a location list, then
     the relevant symtab is that of the .rela.debug_loc section; otherwise
     it's that of the section where the attribute resides.  */

  switch (attr->form)
    {
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_sec_offset:
      reloc->sec = IDX_debug_loc;
      break;

    default:
      reloc->sec = cu_sec_idx (reloc->cu);
      break;
    }

  return 0;
}
