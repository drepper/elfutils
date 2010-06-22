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

#include "libdwP.h"
#include <dwarf.h>

static int
relocatable_form (struct Dwarf_CU *cu,
		  unsigned int sec_idx,
		  unsigned int form,
		  const unsigned char *valp,
		  Dwarf_Addr adjust,
		  GElf_Sym *sym, const char **name,
		  GElf_Sxword *addend, const char **secname)
{
  int width;
  switch (form)
    {
    default:
      /* This can't be relocatable.  We'll let __libdw_relocatable
	 fill in the SHN_ABS indicator for the constant 0 base.  */
      if (addend != NULL)
	{
	  Dwarf_Attribute attr =
	    {
	      .cu = cu,
	      .form = form,
	      .valp = (unsigned char *) valp
	    };
	  if (INTUSE(dwarf_formsdata) (&attr, addend))
	    return -1;
	  *addend += adjust;
	  addend = NULL;
	}
      width = 0;
      valp = NULL;
      break;

    case DW_FORM_addr:
      width = cu->address_size;
      break;

    case DW_FORM_data4:
      width = 4;
      break;

    case DW_FORM_data8:
      width = 8;
      break;
    }

  return __libdw_relocatable (cu->dbg, sec_idx, valp, width,
			      sym, name, addend, adjust, secname);
}

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

  return relocatable_form (reloc->cu, reloc->sec,
			   reloc->form, reloc->valp, reloc->adjust,
			   sym, name, addend, secname);
}

#if 0
/* Shorthand for dwarf_relocatable_info(dwarf_form_relocatable).  */

int
dwarf_form_relocatable_info (attr, sym, name, addend, secname)
     Dwarf_Attribute *attr;
     GElf_Sym *sym;
     const char **name;
     GElf_Sxword *addend;
     const char **secname;
{
  if (attr == NULL)
    return -1;

  return relocatable_form (attr->cu, cu_sec_idx (attr->cu),
			   attr->form, attr->valp, 0,
			   sym, name, addend, secname);
}
#endif
