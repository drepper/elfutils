/* elfutils::dwarf_data common internal templates.
   Copyright (C) 2009 Red Hat, Inc.
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

#include "dwarf_data"

template<typename v, typename subtype>
static inline bool is_a (const typename v::value_dispatch *value)
{
  return dynamic_cast<const subtype *> (value) != NULL;
}

namespace elfutils
{
  template<class impl, typename v>
  dwarf::value_space
  dwarf_data::attr_value<impl, v>::what_space () const
  {
    if (is_a<v, typename v::value_flag> (_m_value))
      return dwarf::VS_flag;
    if (is_a<v, typename v::value_dwarf_constant> (_m_value))
      return dwarf::VS_dwarf_constant;
    if (is_a<v, typename v::value_reference> (_m_value))
      return dwarf::VS_reference;
    if (is_a<v, typename v::value_lineptr> (_m_value))
      return dwarf::VS_lineptr;
    if (is_a<v, typename v::value_macptr> (_m_value))
      return dwarf::VS_macptr;
    if (is_a<v, typename v::value_rangelistptr> (_m_value))
      return dwarf::VS_rangelistptr;
    if (is_a<v, typename v::value_identifier> (_m_value))
      return dwarf::VS_identifier;
    if (is_a<v, typename v::value_string> (_m_value))
      return dwarf::VS_string;
    if (is_a<v, typename v::value_source_file> (_m_value))
      return dwarf::VS_source_file;
    if (is_a<v, typename v::value_source_line> (_m_value))
      return dwarf::VS_source_line;
    if (is_a<v, typename v::value_source_column> (_m_value))
      return dwarf::VS_source_column;
    if (is_a<v, typename v::value_address> (_m_value))
      return dwarf::VS_address;
    if (is_a<v, typename v::value_constant> (_m_value)
	|| is_a<v, typename v::value_constant_block> (_m_value))
      return dwarf::VS_constant;
    if (is_a<v, typename v::value_location> (_m_value))
      return dwarf::VS_location;

    throw std::logic_error ("attr_value has no known value_space!");
  }

  template<typename attr_pair>
  static inline std::string
  attribute_string (const attr_pair &attr)
  {
    std::string result = dwarf::attributes::name (attr.first);
    result += "=";
    result += attr.second.to_string ();
    return result;
  }

  template<typename die_type>
  std::string
  die_string (const die_type &die)
  {
    std::string result ("<");
    result += dwarf::tags::name (die.tag ());

    typename die_type::attributes_type::const_iterator name_attr
      = die.attributes ().find (::DW_AT_name);
    if (name_attr != die.attributes ().end ())
      {
	result += " ";
	result += to_string (*name_attr);
      }

    result += die.has_children () ? ">" : "/>";
    return result;
  }

};
