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

#ifndef DWARFLINT_RELOC_H
#define DWARFLINT_RELOC_H

#include "where.h"
#include "elf_file_i.hh"
#include <libelf.h>
#include <gelf.h>

struct relocation
{
  uint64_t offset;
  uint64_t addend;
  int symndx;
  int type;
  bool invalid;	/* Whether this one relocation should be
		   ignored.  Necessary so that we don't report
		   invalid & missing relocation twice.  */

  relocation ()
    : offset (0)
    , addend (0)
    , symndx (0)
    , type (0)
    , invalid (false)
  {}
};

struct relocation_data
{
  Elf_Data *symdata;       /* Symbol table associated with this
			      relocation section.  */
  size_t type;             /* SHT_REL or SHT_RELA.  */

  struct relocation *rel;  /* Array of relocations.  May be NULL if
			      there are no associated relocation
			      data.  */
  size_t size;
  size_t alloc;
  size_t index;            /* Current index. */

  relocation_data ()
    : symdata (NULL)
    , type (SHT_NULL)
    , rel (NULL)
    , size (0)
    , alloc (0)
    , index (0)
  {}
};

enum skip_type
  {
    skip_unref = 0,
    skip_mismatched = 1,
    skip_ok,
  };

bool read_rel (struct elf_file *file, struct sec *sec,
	       Elf_Data *reldata, bool elf_64);

relocation *relocation_next (struct relocation_data *reloc,
			     uint64_t offset,
			     struct where const *where,
			     enum skip_type st);

void relocation_reset (struct relocation_data *reloc);

void relocation_skip (struct relocation_data *reloc, uint64_t offset,
		      struct where const *where, enum skip_type st);

void relocation_skip_rest (struct relocation_data *reloc,
			   enum section_id id);

void relocate_one (struct elf_file const *file,
		   struct relocation_data *reloc,
		   struct relocation *rel,
		   unsigned width, uint64_t *value,
		   struct where const *where,
		   enum section_id offset_into, GElf_Sym **symptr);

#define PRI_LACK_RELOCATION ": %s seems to lack a relocation.\n"

#endif//DWARFLINT_RELOC_H
