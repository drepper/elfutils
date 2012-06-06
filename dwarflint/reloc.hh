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

#ifndef DWARFLINT_RELOC_H
#define DWARFLINT_RELOC_H

#include "locus.hh"
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

struct rel_target
{
  enum target
    {
      rel_value,	/* For relocations, this denotes that the
			   relocation is applied to target value, not a
			   section offset.  */
      rel_address,	/* Same as above, but for addresses.  */
      rel_exec,		/* Some as above, but we expect EXEC bit.  */
    };

private:
  bool _m_is_section;
  union
  {
    section_id _m_section;
    target _m_target;
  };

public:
  rel_target (section_id sec)
    : _m_is_section (true)
    , _m_section (sec)
  {}

  rel_target (target t)
    : _m_is_section (false)
    , _m_target (t)
  {}

  bool
  operator== (section_id sec)
  {
    return _m_is_section && _m_section == sec;
  }

  bool
  operator== (target tgt)
  {
    return !_m_is_section && _m_target == tgt;
  }

  template<class T>
  bool
  operator!= (T t)
  {
    return !(*this == t);
  }
};

bool read_rel (struct elf_file *file, struct sec *sec,
	       Elf_Data *reldata, bool elf_64);

relocation *relocation_next (struct relocation_data *reloc,
			     uint64_t offset,
			     locus const &loc,
			     enum skip_type st);

void relocation_reset (struct relocation_data *reloc);

void relocation_skip (struct relocation_data *reloc, uint64_t offset,
		      locus const &loc, enum skip_type st);

void relocation_skip_rest (struct relocation_data *reloc,
			   locus const &loc);

void relocate_one (struct elf_file const *file,
		   struct relocation_data *reloc,
		   struct relocation *rel,
		   unsigned width, uint64_t *value,
		   locus const &loc,
		   rel_target reltgt,
		   GElf_Sym **symptr);

#define PRI_LACK_RELOCATION ": %s seems to lack a relocation.\n"

#endif//DWARFLINT_RELOC_H
