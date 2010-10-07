/* Pedantic checking of DWARF files
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

#include "dwarf_version-imp.hh"
#include "check_debug_info.hh"

full_form::full_form (int a_name, dw_class_set a_classes,
		      form_width_t a_width, storage_class_t a_storclass)
  : basic_form (a_name, a_classes)
  , _m_width (a_width)
  , _m_storclass (a_storclass)
{}

form_width_t
full_form::width (__attribute__ ((unused)) struct cu const *cu) const
{
  return _m_width;
}

storage_class_t
full_form::storage_class () const
{
  return _m_storclass;
}

form_width_t
width_off::width (cu const *cu)
{
  return static_cast<form_width_t> (cu->head->offset_size);
}

form_width_t
width_addr::width (struct cu const *cu)
{
  return static_cast<form_width_t> (cu->head->address_size);
}

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
