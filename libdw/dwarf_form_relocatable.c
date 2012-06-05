/* Return relocatable address from attribute.
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
dwarf_form_relocatable (attr, reloc)
     Dwarf_Attribute *attr;
     Dwarf_Relocatable *reloc;
{
  if (attr == NULL)
    return -1;

  *reloc = (Dwarf_Relocatable)
    {
      .sec = cu_sec_idx (attr->cu), .form = attr->form,
      .cu = attr->cu, .valp = attr->valp,
    };

  return 0;
}
INTDEF (dwarf_form_relocatable)
