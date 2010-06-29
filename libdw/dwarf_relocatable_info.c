/* Return relocatable address from attribute.
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
  if (reloc->valp != NULL)
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
