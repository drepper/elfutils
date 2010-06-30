/* Internal definitions for libdw relocation handling.
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

#include "libdwP.h"
#include "libelfP.h"


struct dwarf_reloc_table
{
  size_t n;			/* Number of elements in this table.  */
  const unsigned char **datum;	/* Sorted pointers into section data.  */
  int *symndx;			/* Corresponding symtab indices.  */
  size_t hint;			/* Index of last search.  */
};

struct dwarf_section_reloc
{
  /* This is set only in the laziest state from startup: the reloc
     section needs to be examined, and the rest is uninitialized.
     When this is cleared, the rest is initialized and safe to use.  */
  Elf_Scn *scn;

  Elf_Data *symdata;
  Elf_Data *symstrdata;
  Elf_Data *symxndxdata;

  /* Two tables of predigested relocations, segregated by size.  */
  struct dwarf_reloc_table rel4;
  struct dwarf_reloc_table rel8;

  /* For SHT_RELA, a parallel table of addends.
     For SHT_REL, these are null.  */
  Elf32_Sword *rela4;
  Elf64_Sxword *rela8;
};


extern int __libdw_relocatable (Dwarf *dbg, int sec_index,
				const unsigned char *datum, int width,
				int *symndx, GElf_Sxword *addend)
  __nonnull_attribute__ (1) internal_function;

extern int __libdw_relocatable_getsym (Dwarf *dbg, int sec_index,
				       const unsigned char *datum, int width,
				       int *symndx, GElf_Sym *sym,
				       GElf_Word *shndx, GElf_Sxword *addend)
  __nonnull_attribute__ (1, 3, 5, 6, 7, 8) internal_function;

extern int __libdw_relocate_shndx (Dwarf *dbg,
				   GElf_Word shndx, GElf_Sxword addend,
				   Dwarf_Addr *val)
  __nonnull_attribute__ (1, 4) internal_function;

extern ptrdiff_t __libdw_ranges_relocatable (struct Dwarf_CU *cu,
					     Dwarf_Attribute *attr,
					     ptrdiff_t offset,
					     Dwarf_Relocatable *basep,
					     Dwarf_Relocatable *startp,
					     Dwarf_Relocatable *endp,
					     Dwarf_Block *exprloc)
  __nonnull_attribute__ (1, 4, 5, 6) internal_function;

extern void __libdwfl_relocate_setup (Dwarf *dbg, struct dwarf_section_reloc *r)
  __nonnull_attribute__ (1, 2) internal_function;

extern int __libdwfl_relocate_symbol (struct dwarf_section_reloc *r, bool undef,
				      Dwarf *dbg, GElf_Sym *sym,
				      GElf_Word shndx, GElf_Sxword *addend)
  __nonnull_attribute__ (1, 3, 4) internal_function;
