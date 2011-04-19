/*
   Copyright (C) 2011 Red Hat, Inc.
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

#ifndef _DWARFLINT_DIE_LOCUS_H_
#define _DWARFLINT_DIE_LOCUS_H_

#include "locus.hh"
#include "../libdw/c++/dwarf"

namespace locus_simple_fmt
{
  char const *cu_n ();
};

typedef fixed_locus<sec_info,
		    locus_simple_fmt::cu_n,
		    locus_simple_fmt::dec> cu_locus;

class die_locus
  : public clonable_locus<die_locus>
{
  Dwarf_Off _m_offset;
  int _m_attrib_name;

public:
  explicit die_locus (Dwarf_Off offset, int attrib_name = -1)
    : _m_offset (offset)
    , _m_attrib_name (attrib_name)
  {}

  template <class T>
  explicit die_locus (T const &die, int attrib_name = -1)
    : _m_offset (die.offset ())
    , _m_attrib_name (attrib_name)
  {}

  void
  set_attrib_name (int attrib_name)
  {
    _m_attrib_name = attrib_name;
  }

  std::string format (bool brief = false) const;
};

#endif /* _DWARFLINT_DIE_LOCUS_H_ */
