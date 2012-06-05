/* Determine whether a DIE covers a PC address.
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
dwarf_haspc_relocatable (Dwarf_Die *die, Dwarf_Relocatable *pc)
{
  if (die == NULL)
    return -1;

  GElf_Sym pc_sym;
  GElf_Sxword pc_addend;
  int pc_shndx = INTUSE(dwarf_relocatable_info) (pc, &pc_sym, NULL,
						 &pc_addend, NULL);
  if (pc_shndx < 0)
    return pc_shndx;
  pc_sym.st_value += pc_addend;

  Dwarf_Relocatable base;
  Dwarf_Relocatable begin;
  Dwarf_Relocatable end;
  ptrdiff_t offset = 0;
  while ((offset = INTUSE(dwarf_ranges_relocatable) (die, offset, &base,
						     &begin, &end)) > 0)
    if (begin.valp == NULL && end.valp == NULL && pc->valp == NULL)
      {
	if (pc->adjust >= begin.adjust && pc->adjust < end.adjust)
	  return 1;
      }
    else
      {
	/* A relocatable address matches if it's in the same section
	   and the section-relative offsets match.  */

	GElf_Sym sym;
	GElf_Sxword addend;
	int shndx = INTUSE(dwarf_relocatable_info) (&begin, &sym, NULL,
						    &addend, NULL);
	if (shndx < 0)
	  return -1;
	if (shndx == pc_shndx && sym.st_shndx == pc_sym.st_shndx
	    && pc_sym.st_value >= sym.st_value + addend)
	  {
	    if (pc_sym.st_value == sym.st_value + addend)
	      return 1;
	    shndx = INTUSE(dwarf_relocatable_info) (&end, &sym, NULL,
						    &addend, NULL);
	    if (shndx < 0)
	      return -1;
	    if (shndx == pc_shndx && sym.st_shndx == pc_sym.st_shndx
		&& pc_sym.st_value < sym.st_value + addend)
	      return 1;
	  }
      }

  return offset;
}
INTDEF (dwarf_haspc_relocatable)
