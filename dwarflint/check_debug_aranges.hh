/* Low-level checking of .debug_aranges.
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

#ifndef DWARFLINT_CHECK_DEBUG_ARANGES_HH
#define DWARFLINT_CHECK_DEBUG_ARANGES_HH

#include "checks.hh"
#include "sections_i.hh"
#include "check_debug_info_i.hh"
#include "cu_coverage_i.hh"

namespace locus_simple_fmt
{
  char const *cudie_n ();
};

class cudie_locus
  : public fixed_locus<sec_info,
		       locus_simple_fmt::cudie_n,
		       locus_simple_fmt::dec>
{
  typedef fixed_locus<sec_info,
		      locus_simple_fmt::cudie_n,
		      locus_simple_fmt::dec> _super_t;
public:
  template <class T>
  cudie_locus (T const &die)
    : _super_t (die.offset ())
  {}

  cudie_locus (Dwarf_Off offset)
    : _super_t (offset)
  {}
};

class arange_locus
  : public locus
{
  Dwarf_Off _m_table_offset;
  Dwarf_Off _m_arange_offset;
  locus const *_m_cudie_locus;

public:
  explicit arange_locus (Dwarf_Off table_offset = -1,
			 Dwarf_Off arange_offset = -1)
    : _m_table_offset (table_offset)
    , _m_arange_offset (arange_offset)
    , _m_cudie_locus (NULL)
  {}

  explicit arange_locus (locus const &cudie_locus)
    : _m_table_offset (-1)
    , _m_arange_offset (-1)
    , _m_cudie_locus (&cudie_locus)
  {}

  void
  set_cudie (locus const *cudie_locus)
  {
    _m_cudie_locus = cudie_locus;
  }

  void
  set_arange (Dwarf_Off arange_offset)
  {
    _m_arange_offset = arange_offset;
  }

  std::string format (bool brief = false) const;
};

class check_debug_aranges
  : public check<check_debug_aranges>
{
  section<sec_aranges> *_m_sec_aranges;
  check_debug_info *_m_info;
  cu_coverage *_m_cu_coverage;

public:
  static checkdescriptor const *descriptor ();
  check_debug_aranges (checkstack &stack, dwarflint &lint);
};

#endif//DWARFLINT_CHECK_DEBUG_ARANGES_HH
