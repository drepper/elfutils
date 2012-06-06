/* Low-level checking of .debug_line
   Copyright (C) 2010, 2011 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECK_DEBUG_LINE_HH
#define DWARFLINT_CHECK_DEBUG_LINE_HH

#include "check_debug_info_i.hh"
#include "sections_i.hh"
#include "checks.hh"

#include "../libdw/libdw.h"
#include <set>

class check_debug_line
  : public check<check_debug_line>
{
  section<sec_line> *_m_sec;
  check_debug_info *_m_info;
  std::set<Dwarf_Off> _m_line_tables;

public:
  static checkdescriptor const *descriptor ();
  check_debug_line (checkstack &stack, dwarflint &lint);

  std::set<Dwarf_Off> const &line_tables () const { return _m_line_tables; }

  bool has_line_table (Dwarf_Off off) const;
};

#endif//DWARFLINT_CHECK_DEBUG_LINE_HH
