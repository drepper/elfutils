/* Low-level section handling.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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

#ifndef DWARFLINT_SECTIONS_HH
#define DWARFLINT_SECTIONS_HH

#include "checks.hh"
#include "elf_file.hh"

class load_sections
  : public check<load_sections>
{
public:
  static checkdescriptor const *descriptor ();

  elf_file file;
  load_sections (checkstack &stack, dwarflint &lint);
  ~load_sections ();
};

class section_base
{
  load_sections *sections;
  sec &get_sec_or_throw (section_id secid);

public:
  static checkdescriptor const *descriptor ();

  sec &sect;
  elf_file &file;
  section_base (checkstack &stack,
		dwarflint &lint, section_id secid);

  relocation_data *reldata () const
  {
    return sect.rel.size > 0 ? &sect.rel : NULL;
  }
};

template<section_id sec_id>
class section
  : public section_base
  , public check<section<sec_id> >
{
public:
  static checkdescriptor const *descriptor () {
    static checkdescriptor cd
      (checkdescriptor::create (section_name[sec_id])
       .hidden ());
    return &cd;
  }

  explicit section (checkstack &stack, dwarflint &lint)
    : section_base (stack, lint, sec_id)
  {}
};

#endif//DWARFLINT_SECTIONS_HH
