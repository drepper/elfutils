/* Low-level checking of .debug_pub*.
   Copyright (C) 2009 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECK_DEBUG_PUB_HH
#define DWARFLINT_CHECK_DEBUG_PUB_HH

#include "sections_i.hh"
#include "check_debug_info_i.hh"
#include "checks.hh"
#include "elf_file_i.hh"

template<section_id sec_id>
class check_debug_pub
  : public check<check_debug_pub<sec_id> >
{
protected:
  typedef section<sec_id> section_t;
  section_t *_m_sec;
  elf_file const &_m_file;
  check_debug_info *_m_cus;

public:
  // instantiated in .cc for each subclass
  check_debug_pub (checkstack &stack, dwarflint &lint);
};

struct check_debug_pubnames
  : public check_debug_pub<sec_pubnames>
{
  static checkdescriptor const *descriptor ();

  check_debug_pubnames (checkstack &stack, dwarflint &lint)
    : check_debug_pub<sec_pubnames> (stack, lint)
  {}
};

struct check_debug_pubtypes
  : public check_debug_pub<sec_pubtypes>
{
  static checkdescriptor const *descriptor ();

  check_debug_pubtypes (checkstack &stack, dwarflint &lint)
    : check_debug_pub<sec_pubtypes> (stack, lint)
  {}
};

#endif//DWARFLINT_CHECK_DEBUG_PUB_HH
