/* Pedantic checking of DWARF files
   Copyright (C) 2010 Red Hat, Inc.
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

#include "dwarf_version-imp.hh"

template <class T>
void
dwver_index_table<T>::add (T const &emt)
{
  _m_table.insert (std::make_pair (emt.name (), emt));
}

template <class T>
T const *
dwver_index_table<T>::get (int f) const
{
  typename _table_t::const_iterator it = _m_table.find (f);
  if (it != _m_table.end ())
    return &it->second;
  else
    return NULL;
}

template class dwver_index_table<form>;
template class dwver_index_table<attribute>;

offset_form::offset_form (int a_name, dw_class_set a_classes)
  : form (a_name, a_classes, fw_offset, sc_value)
{}

address_form::address_form (int a_name, dw_class_set a_classes)
  : form (a_name, a_classes, fw_address, sc_value)
{}

string_form::string_form (int a_name)
  : preset_form<sc_string, cl_string> (a_name, fw_unknown)
{}

std_dwarf::std_dwarf (attribute_table const &attrtab,
		      form_table const &formtab)
  : _m_attrtab (attrtab)
  , _m_formtab (formtab)
{}

form const *
std_dwarf::get_form (int form_name) const
{
  return _m_formtab.get (form_name);
}

attribute const *
std_dwarf::get_attribute (int attribute_name) const
{
  return _m_attrtab.get (attribute_name);
}
