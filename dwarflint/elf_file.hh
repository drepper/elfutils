/* Pedantic checking of DWARF files.
   Copyright (C) 2008, 2009, 2010, 2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef DWARFLINT_ELF_FILE_HH
#define DWARFLINT_ELF_FILE_HH

#include "../libdw/libdw.h"
#include "../libebl/libebl.h"
#include "reloc.hh"

struct sec
{
  GElf_Shdr shdr;
  struct relocation_data rel;
  Elf_Scn *scn;
  const char *name;

  Elf_Data *data;	/* May be NULL if data in this section are
			   missing or not substantial.  */
  enum section_id id;

  sec ()
    : scn (NULL)
    , name (NULL)
    , data (NULL)
    , id (sec_invalid)
  {}
};

struct elf_file
{
  GElf_Ehdr ehdr;	/* Header of underlying Elf.  */
  Elf *elf;
  Ebl *ebl;

  struct sec *sec;	/* Array of sections.  */
  size_t size;
  size_t alloc;

  /* Pointers into SEC above.  Maps section_id to section.  */
  struct sec *debugsec[count_debuginfo_sections];

  bool addr_64;	/* True if it's 64-bit Elf.  */
  bool other_byte_order; /* True if the file has a byte order
			    different from the host.  */
};

#endif/*DWARFLINT_ELF_FILE_HH*/
