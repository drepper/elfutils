/* Internal definitions for libdw relocation handling.
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
