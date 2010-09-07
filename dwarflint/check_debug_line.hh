/* Low-level checking of .debug_line
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

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifndef DWARFLINT_CHECK_DEBUG_LINE_HH
#define DWARFLINT_CHECK_DEBUG_LINE_HH

#include "check_debug_info.ii"
#include "sections.ii"
#include "checks.hh"
#include <set>
#include "low.h"

class check_debug_line
  : public check<check_debug_line>
{
  section<sec_line> *_m_sec;
  check_debug_info *_m_info;
  std::set<Dwarf_Off> _m_line_tables;

public:
  static checkdescriptor const &descriptor ();
  check_debug_line (checkstack &stack, dwarflint &lint);

  std::set<Dwarf_Off> const &line_tables () const { return _m_line_tables; }

  bool
  has_line_table (Dwarf_Off off) const
  {
    return _m_line_tables.find (off) != _m_line_tables.end ();
  }
};

#endif//DWARFLINT_CHECK_DEBUG_LINE_HH
