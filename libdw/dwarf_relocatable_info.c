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

#include "relocate.h"
#include <dwarf.h>


int
dwarf_relocatable_info (reloc, sym, name, addend, secname)
     Dwarf_Relocatable *reloc;
     GElf_Sym *sym;
     const char **name;
     GElf_Sxword *addend;
     const char **secname;
{
  if (reloc == NULL)
    return -1;

  int width;
  switch (reloc->form)
    {
    default:
      /* This can't be relocatable.  */
      if (addend != NULL)
	{
	  if (reloc->valp == NULL)
	    *addend = 0;
	  else
	    {
	      Dwarf_Attribute attr =
		{
		  .cu = reloc->cu,
		  .form = reloc->form,
		  .valp = (unsigned char *) reloc->valp
		};
	      if (INTUSE(dwarf_formsdata) (&attr, addend))
		return -1;
	    }
	}
    noreloc:
      if (addend != NULL)
	*addend += reloc->adjust;
      if (sym != NULL)
	*sym = (GElf_Sym) { .st_shndx = SHN_ABS };
      if (name != NULL)
	*name = NULL;
      if (secname != NULL)
	*secname = NULL;
      return 0;

    case DW_FORM_addr:
      width = reloc->cu->address_size;
      break;

    case DW_FORM_data4:
      width = 4;
      break;

    case DW_FORM_data8:
      width = 8;
      break;
    }

  GElf_Sxword adjust = 0;
  if (reloc->valp != NULL && reloc->symndx == STN_UNDEF)
    {
      int result = __libdw_relocatable (reloc->cu->dbg, reloc->sec,
					reloc->valp, width,
					&reloc->symndx, &adjust);
      if (unlikely (result < 0))
	return result;
      if (result == 0)
	{
	  reloc->valp = NULL;
	  reloc->adjust += adjust;
	  goto noreloc;
	}
    }

  struct dwarf_section_reloc *const r
    = reloc->cu->dbg->relocate->sectionrel[reloc->sec];
  GElf_Sym sym_mem;
  GElf_Word shndx;
  if (sym == NULL)
    sym = &sym_mem;

  if (unlikely (gelf_getsymshndx (r->symdata, r->symxndxdata,
				  reloc->symndx, sym, &shndx) == NULL))
    {
      __libdw_seterrno (DWARF_E_RELBADSYM);
      return -1;
    }

  if (reloc->valp != NULL)
    {
      /* Turn the adjustment into a section-relative address.  */
      reloc->valp = NULL;
      reloc->adjust += sym->st_value + adjust;
    }

  if (name != NULL)
    {
      if (sym->st_name == 0)
	*name = NULL;
      else if (unlikely (sym->st_name >= r->symstrdata->d_size))
	{
	  __libdw_seterrno (DWARF_E_RELBADSYM);
	  return -1;
	}
      else
	*name = (const char *) r->symstrdata->d_buf + sym->st_name;
    }

  if (addend != NULL)
    /* The adjustment is already section-relative, so we have to
       remove the st_value portion of it.  */
    *addend = reloc->adjust - sym->st_value;

  int result = (sym->st_shndx < SHN_LORESERVE ? sym->st_shndx
		: sym->st_shndx == SHN_XINDEX ? shndx : SHN_UNDEF);

  if (secname != NULL)
    {
      Elf *symelf = ((Elf_Data_Scn *) r->symdata)->s->elf;
      size_t shstrndx;
      GElf_Shdr shdr;
      if (result == 0
	  || elf_getshdrstrndx (symelf, &shstrndx) < 0
	  || gelf_getshdr (elf_getscn (symelf, result), &shdr) == NULL)
	*secname = NULL;
      else
	*secname = elf_strptr (symelf, shstrndx, shdr.sh_name);
    }

  return result;
}
INTDEF (dwarf_relocatable_info)
