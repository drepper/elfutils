/* Return line address.
   Copyright (C) 2004-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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
#include "relocate.h"


int
dwarf_lineaddr (Dwarf_Line *line, Dwarf_Addr *addrp)
{
  if (line == NULL)
    return -1;

  const int *const reloc = line->cu->lines->reloc;
  const size_t idx = line - line->cu->lines->info;

  if (reloc == NULL || reloc[idx * 2] == STN_UNDEF)
    {
      *addrp = line->addr;
      return 0;
    }

  /* We have already reduced this relocatable address to a section offset.
     We just have to resolve the section address.  */

  return __libdw_relocate_shndx (line->cu->dbg,
				 reloc[idx * 2 + 1], line->addr, addrp);
}
INTDEF (dwarf_lineaddr)
