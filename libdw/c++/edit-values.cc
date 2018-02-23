/* elfutils::dwarf_edit attribute value interfaces.
   Copyright (C) 2009-2010 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "dwarf_edit"
#include "data-values.hh"

using namespace elfutils;

// Explicit instantiations.
template class dwarf_data::line_entry<dwarf_edit::source_file>;
template class dwarf_data::line_table<dwarf_edit::line_entry>;
template class dwarf_data::line_info_table<dwarf_edit::line_table>;
template class dwarf_data::attr_value<dwarf_edit>;
template class dwarf_data::value<dwarf_edit>;

template<>
std::string
elfutils::to_string<dwarf_edit::attribute> (const dwarf_edit::attribute &attr)
{
  return attribute_string (attr);
}

namespace elfutils
{
  template<>
  std::string to_string (const dwarf_edit::debug_info_entry &die)
  {
    return die_string (die);
  }
};

std::string
dwarf_data::source_file::to_string () const
{
  if (likely (_m_mtime == 0) && likely (_m_size == 0))
    return "\"" + _m_name + "\"";

  std::ostringstream os;
  os << "{\"" << _m_name << "," << _m_mtime << "," << _m_size << "}";
  return os.str ();
}
