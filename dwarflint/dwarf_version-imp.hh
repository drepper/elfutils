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

#ifndef DWARFLINT_DWARF_VERSION_IMP_HH
#define DWARFLINT_DWARF_VERSION_IMP_HH

#include "dwarf_version.hh"
#include <map>

template <class T>
class dwver_index_table
{
  typedef std::map<int, T> _table_t;
  _table_t _m_table;

protected:
  void add (T const &emt);

public:
  T const *get (int name) const;
};

typedef dwver_index_table<form> form_table;
typedef dwver_index_table<attribute> attribute_table;

template<storage_class_t StorClass, dw_class... Classes>
struct preset_form
  : public form
{
  preset_form (int a_name, form_width_t a_width)
    : form (a_name, dw_class_set (Classes...), a_width, StorClass)
  {}
};

template<dw_class... Classes>
struct preset_attribute
  : public attribute
{
  preset_attribute (int a_name)
    : attribute (a_name, dw_class_set (Classes...))
  {}
};


struct offset_form
  : public form
{
  offset_form (int a_name, dw_class_set a_classes);
};

struct address_form
  : public form
{
  address_form (int a_name, dw_class_set a_classes);
};

struct string_form
  : public preset_form<sc_string, cl_string>
{
  string_form (int a_name);
};

typedef preset_form<sc_block, cl_block> block_form;
typedef preset_form<sc_value, cl_constant> const_form;
typedef preset_form<sc_value, cl_reference> ref_form;
typedef preset_form<sc_value, cl_flag> flag_form;

typedef preset_attribute<cl_constant> const_attribute;
typedef preset_attribute<cl_reference> ref_attribute;
typedef preset_attribute<cl_address> addr_attribute;
typedef preset_attribute<cl_string> string_attribute;
typedef preset_attribute<cl_flag> flag_attribute;
typedef preset_attribute<cl_block> block_attribute;

// [DWARF 3, DWARF 4, section 2.19]: attributes that [...] specify a
// property [...] that is an integer value, where the value may be
// known during compilation or may be computed dynamically during
// execution.
typedef preset_attribute<cl_constant, cl_exprloc,
			 cl_reference> dynval_attribute;

typedef preset_attribute<cl_exprloc, cl_loclistptr> location_attribute;
typedef preset_attribute<cl_exprloc, cl_reference> static_location_attribute;

class std_dwarf
  : public dwarf_version
{
  attribute_table const _m_attrtab;
  form_table const _m_formtab;

public:
  std_dwarf (attribute_table const &attrtab,
	     form_table const &formtab);

  form const *get_form (int form_name) const;
  attribute const *get_attribute (int attribute_name) const;
};

#endif//DWARFLINT_DWARF_VERSION_IMP_HH
