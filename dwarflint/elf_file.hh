/* Pedantic checking of DWARF files.
   Copyright (C) 2008, 2009, 2010, 2011 Red Hat, Inc.
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

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

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
