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
  typedef std::map<int, T const *> _map_t;
  _map_t _m_data;

protected:
  void add (T const *f);

public:
  T const *get (int f) const;
};

typedef dwver_index_table<form> form_table;
typedef dwver_index_table<attribute> attribute_table;

template<class T>
class dwver_basic
  : public T
{
  int _m_name;
  dw_class_set _m_classes;

public:
  dwver_basic (int a_name, dw_class_set a_classes);

  dw_class_set const &classes () const;
  int name () const;
};

typedef dwver_basic<form> basic_form;
typedef dwver_basic<attribute> basic_attribute;

class full_form
  : public basic_form
{
protected:
  form_width_t _m_width;
  storage_class_t _m_storclass;

public:
  full_form (int a_name, dw_class_set a_classes,
	     form_width_t a_width, storage_class_t a_storclass);

  form_width_t width (cu const *cu = NULL) const;
  storage_class_t storage_class () const;
};

struct offset_form
  : public basic_form
{
  offset_form (int a_name, dw_class_set a_classes);

  form_width_t width (cu const *cu) const;
  storage_class_t storage_class () const;
};

struct address_form
  : public basic_form
{
  address_form (int a_name, dw_class_set a_classes);

  form_width_t width (cu const *cu) const;
  storage_class_t storage_class () const;
};

template<storage_class_t StorClass, dw_class... Classes>
struct preset_form
  : public full_form
{
  preset_form (int a_name, form_width_t a_width)
    : full_form (a_name, dw_class_set (Classes...), a_width, StorClass)
  {}
};

struct string_form
  : public preset_form<sc_string, cl_string>
{
  string_form (int a_name);
};

template<dw_class... Classes>
struct preset_attribute
  : public basic_attribute
{
  preset_attribute (int a_name)
    : basic_attribute (a_name, dw_class_set (Classes...))
  {}
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
typedef preset_attribute<cl_block, cl_constant> const_or_block_attribute;
typedef preset_attribute<cl_block, cl_reference> ref_or_block_attribute;
typedef preset_attribute<cl_reference, cl_constant> const_or_ref_attribute;

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
