/* Determine whether a DIE covers a PC address.
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
