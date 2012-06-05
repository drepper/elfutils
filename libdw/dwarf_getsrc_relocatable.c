/* Find line information for relocatable address.
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
#include <assert.h>


Dwarf_Line *
dwarf_getsrc_relocatable (Dwarf_Die *cudie, Dwarf_Relocatable *reloc)
{
  Dwarf_Lines *lines;
  size_t nlines;

  if (INTUSE(dwarf_getsrclines) (cudie, &lines, &nlines) != 0)
    return NULL;

  /* First we can partially resolve the relocatable address to
     a symndx and a section-relative offset.  */
  GElf_Sym sym;
  GElf_Word shndx;
  if (reloc->valp != NULL)
    {
      int result = INTUSE(dwarf_relocatable_info) (reloc,
						   NULL, NULL, NULL, NULL);
      if (unlikely (result < 0))
	return NULL;
      assert (reloc->valp == NULL);
      shndx = result;
    }
  else if (reloc->symndx == STN_UNDEF)
    shndx = 0;
  else
    {
      /* The relocation is already resolved to a symndx and
	 section-relative address, but we have to recover that shndx.  */
      struct dwarf_section_reloc *const r
	= reloc->cu->dbg->relocate->sectionrel[reloc->sec];
      if (unlikely (gelf_getsymshndx (r->symdata, r->symxndxdata,
				      reloc->symndx, &sym, &shndx) == NULL))
	{
	  __libdw_seterrno (DWARF_E_RELBADSYM);
	  return NULL;
	}
      if (sym.st_shndx == SHN_ABS)
	shndx = 0;
      else if (likely (sym.st_shndx < SHN_LORESERVE)
	       && likely (sym.st_shndx != SHN_UNDEF))
	shndx = sym.st_shndx;
      else if (sym.st_shndx != SHN_XINDEX)
	{
	  __libdw_seterrno (DWARF_E_RELUNDEF);
	  return NULL;
	}
    }

  if (lines->reloc == NULL)
    {
      if (shndx == 0)
	return INTUSE(dwarf_getsrc_die) (cudie, reloc->adjust);
      else
	goto nomatch;
    }

  /* The lines are sorted by address, so we can use binary search.  */
  const Dwarf_Addr addr = reloc->adjust;
  size_t l = 0, u = nlines;
  while (l < u)
    {
      size_t idx = (l + u) / 2;
      const GElf_Word this_shndx = lines->reloc[idx * 2 + 1];
      if (shndx < this_shndx)
	u = idx;
      else if (shndx > this_shndx)
	l = idx + 1;
      else if (addr < lines->info[idx].addr)
	u = idx;
      else if (addr > lines->info[idx].addr || lines->info[idx].end_sequence)
	l = idx + 1;
      else
	return &lines->info[idx];
    }

  if (nlines > 0)
    assert (lines->info[nlines - 1].end_sequence);

  /* If none were equal, the closest one below is what we want.  We
     never want the last one, because it's the end-sequence marker
     with an address at the high bound of the CU's code.  If the debug
     information is faulty and no end-sequence marker is present, we
     still ignore it.  */
  if (u > 0 && u < nlines && addr > lines->info[u - 1].addr)
    {
      while (lines->info[u - 1].end_sequence && u > 0)
	--u;
      if (u > 0)
	return &lines->info[u - 1];
    }

 nomatch:
  __libdw_seterrno (DWARF_E_ADDR_OUTOFRANGE);
  return NULL;
}
