/* Low-level checking of .debug_abbrev.
   Copyright (C) 2009 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECK_DEBUG_ABBREV_HH
#define DWARFLINT_CHECK_DEBUG_ABBREV_HH

#include "low.h"
#include "checks.hh"
#include "sections.ii"

class check_debug_abbrev
  : public check<check_debug_abbrev>
{
  section<sec_abbrev> *_m_sec_abbr;

public:
  static checkdescriptor &descriptor ();

  // offset -> abbreviations
  typedef std::map< ::Dwarf_Off, abbrev_table> abbrev_map;
  abbrev_map const abbrevs;

  check_debug_abbrev (checkstack &stack, dwarflint &lint);
  ~check_debug_abbrev ();
};

#endif//DWARFLINT_CHECK_DEBUG_ABBREV_HH
