/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

// The tables here capture attribute/allowed forms depending on DWARF
// version.  Apart from standardized DWARF formats, e.g. DWARF3+GNU is
// a version in its own.

#include "tables.hh"
#include "check_debug_info.hh"

#include "../libdw/dwarf.h"
#include <map>
#include <cassert>

dw_class_set::dw_class_set (dw_class a, dw_class b, dw_class c,
			    dw_class d, dw_class e)
{
#define ADD(V) if (V != max_dw_class) (*this)[V] = true
  ADD (a);
  ADD (b);
  ADD (c);
  ADD (d);
  ADD (e);
#undef ADD
}

namespace
{
  template <class T>
  class index_table
  {
    typedef std::map<int, T const *> _map_t;
    _map_t _m_data;

  protected:
    void add (T const *f)
    {
      _m_data[f->name ()] = f;
    }

  public:
    T const *
    get (int f) const
    {
      typename _map_t::const_iterator it = _m_data.find (f);
      if (it != _m_data.end ())
	return it->second;
      else
	return NULL;
    }
  };

  typedef index_table<form> form_table;
  typedef index_table<attribute> attribute_table;

  template<class T>
  class basic
    : public T
  {
    int _m_name;
    dw_class_set _m_classes;

  public:
    basic (int a_name, dw_class_set a_classes)
      : _m_name (a_name)
      , _m_classes (a_classes)
    {}

    dw_class_set const &
    classes () const
    {
      return _m_classes;
    }

    int
    name () const
    {
      return _m_name;
    }
  };

  typedef basic<form> basic_form;
  typedef basic<attribute> basic_attribute;

  class full_form
    : public basic_form
  {
  protected:
    form_width_t _m_width;
    storage_class_t _m_storclass;

  public:
    full_form (int a_name, dw_class_set a_classes,
	       form_width_t a_width, storage_class_t a_storclass)
      : basic_form (a_name, a_classes)
      , _m_width (a_width)
      , _m_storclass (a_storclass)
    {}

    form_width_t
    width (__attribute__ ((unused)) struct cu const *cu = NULL) const
    {
      return _m_width;
    }

    storage_class_t
    storage_class () const
    {
      return _m_storclass;
    }
  };

  struct width_off {
    static form_width_t width (struct cu const *cu) {
      return static_cast<form_width_t> (cu->head->offset_size);
    }
  };

  struct width_addr {
    static form_width_t width (struct cu const *cu) {
      return static_cast<form_width_t> (cu->head->address_size);
    }
  };

  template<class WidthSel, storage_class_t StorClass>
  class selwidth_form
    : public basic_form
  {
  public:
    template <class... Clss>
    selwidth_form (int a_name, Clss... a_classes)
      : basic_form (a_name, dw_class_set (a_classes...))
    {}

    form_width_t
    width (struct cu const *cu) const
    {
      return WidthSel::width (cu);
    }

    storage_class_t
    storage_class () const
    {
      return StorClass;
    }
  };

  typedef selwidth_form<width_off, sc_value> offset_form;
  typedef selwidth_form<width_addr, sc_value> address_form;

  template<storage_class_t StorClass, dw_class... Classes>
  struct preset_form
    : public full_form
  {
    preset_form (int a_name, form_width_t a_width)
      : full_form (a_name, dw_class_set (Classes...), a_width, StorClass)
    {}
  };

  typedef preset_form<sc_block, cl_block> block_form;
  typedef preset_form<sc_value, cl_constant> const_form;
  typedef preset_form<sc_value, cl_reference> ref_form;
  typedef preset_form<sc_value, cl_flag> flag_form;

  template<dw_class... Classes>
  struct preset_attribute
    : public basic_attribute
  {
    preset_attribute (int a_name)
      : basic_attribute (a_name, dw_class_set (Classes...))
    {}
  };

  typedef preset_attribute<cl_constant> const_attribute;
  typedef preset_attribute<cl_reference> ref_attribute;
  typedef preset_attribute<cl_address> addr_attribute;
  typedef preset_attribute<cl_string> string_attribute;
  typedef preset_attribute<cl_flag> flag_attribute;
  typedef preset_attribute<cl_block> block_attribute;
  typedef preset_attribute<cl_block, cl_constant> const_or_block_attribute;
  typedef preset_attribute<cl_block, cl_reference> ref_or_block_attribute;
  typedef preset_attribute<cl_reference, cl_constant> const_or_ref_attribute;

  struct dwarf_row {
    int val;
    dw_class_set classes;
  };

  struct dwarf_2_attributes
    : public attribute_table
  {
    dwarf_2_attributes ()
    {
      add (new ref_attribute (DW_AT_sibling));
      add (new const_or_block_attribute (DW_AT_location));
      add (new string_attribute (DW_AT_name));
      add (new const_attribute (DW_AT_ordering));
      add (new const_attribute (DW_AT_byte_size));
      add (new const_attribute (DW_AT_bit_offset));
      add (new const_attribute (DW_AT_bit_size));
      add (new const_attribute (DW_AT_stmt_list));
      add (new addr_attribute (DW_AT_low_pc));
      add (new addr_attribute (DW_AT_high_pc));
      add (new const_attribute (DW_AT_language));
      add (new ref_attribute (DW_AT_discr));
      add (new const_attribute (DW_AT_discr_value));
      add (new const_attribute (DW_AT_visibility));
      add (new ref_attribute (DW_AT_import));
      add (new const_or_block_attribute (DW_AT_string_length));
      add (new ref_attribute (DW_AT_common_reference));
      add (new string_attribute (DW_AT_comp_dir));
      add (new basic_attribute (DW_AT_const_value,
				dw_class_set (cl_string, cl_constant,
					      cl_block)));
      add (new ref_attribute (DW_AT_containing_type));
      add (new ref_attribute (DW_AT_default_value));
      add (new const_attribute (DW_AT_inline));
      add (new flag_attribute (DW_AT_is_optional));
      add (new const_or_ref_attribute (DW_AT_lower_bound));
      add (new string_attribute (DW_AT_producer));
      add (new flag_attribute (DW_AT_prototyped));
      add (new const_or_block_attribute (DW_AT_return_addr));
      add (new const_attribute (DW_AT_start_scope));
      add (new const_attribute (DW_AT_bit_stride));
      add (new const_or_ref_attribute (DW_AT_upper_bound));
      add (new const_attribute (DW_AT_abstract_origin));
      add (new ref_attribute (DW_AT_accessibility));
      add (new const_attribute (DW_AT_address_class));
      add (new flag_attribute (DW_AT_artificial));
      add (new ref_attribute (DW_AT_base_types));
      add (new const_attribute (DW_AT_calling_convention));
      add (new const_or_ref_attribute (DW_AT_count));
      add (new ref_or_block_attribute (DW_AT_data_member_location));
      add (new const_attribute (DW_AT_decl_column));
      add (new const_attribute (DW_AT_decl_file));
      add (new const_attribute (DW_AT_decl_line));
      add (new flag_attribute (DW_AT_declaration));
      add (new block_attribute (DW_AT_discr_list));
      add (new const_attribute (DW_AT_encoding));
      add (new flag_attribute (DW_AT_external));
      add (new const_or_block_attribute (DW_AT_frame_base));
      add (new ref_attribute (DW_AT_friend));
      add (new const_attribute (DW_AT_identifier_case));
      add (new const_attribute (DW_AT_macro_info));
      add (new block_attribute (DW_AT_namelist_item));
      add (new ref_attribute (DW_AT_priority));
      add (new const_or_block_attribute (DW_AT_segment));
      add (new ref_attribute (DW_AT_specification));
      add (new const_or_block_attribute (DW_AT_static_link));
      add (new ref_attribute (DW_AT_type));
      add (new const_or_block_attribute (DW_AT_use_location));
      add (new flag_attribute (DW_AT_variable_parameter));
      add (new const_attribute (DW_AT_virtuality));
      add (new ref_or_block_attribute (DW_AT_vtable_elem_location));
    }
  };

  struct string_form
    : public preset_form<sc_string, cl_string>
  {
    string_form (int a_name)
      : preset_form<sc_string, cl_string> (a_name, fw_unknown)
    {}
  };

  struct dwarf_2_forms
    : public form_table
  {
    dwarf_2_forms ()
    {
      add (new block_form (DW_FORM_block, fw_uleb));
      add (new block_form (DW_FORM_block1, fw_1));
      add (new block_form (DW_FORM_block2, fw_2));
      add (new block_form (DW_FORM_block4, fw_4));

      add (new const_form (DW_FORM_data1, fw_1));
      add (new const_form (DW_FORM_data2, fw_2));
      add (new const_form (DW_FORM_data4, fw_4));
      add (new const_form (DW_FORM_data8, fw_8));
      add (new const_form (DW_FORM_sdata, fw_sleb));
      add (new const_form (DW_FORM_udata, fw_uleb));

      add (new flag_form (DW_FORM_flag, fw_1));

      add (new ref_form (DW_FORM_ref1, fw_1));
      add (new ref_form (DW_FORM_ref2, fw_2));
      add (new ref_form (DW_FORM_ref4, fw_4));
      add (new ref_form (DW_FORM_ref8, fw_8));
      add (new ref_form (DW_FORM_ref_udata, fw_uleb));

      add (new string_form (DW_FORM_string));
      add (new offset_form (DW_FORM_strp, cl_string));
      add (new address_form (DW_FORM_addr, cl_address));
      add (new address_form (DW_FORM_ref_addr, cl_reference));
    }
  };

  typedef preset_attribute<cl_block, cl_loclistptr> block_or_loc_attribute;
  typedef preset_attribute<cl_block, cl_constant, cl_reference>
  block_const_ref_attribute;

  struct dwarf_3_attributes
    : public attribute_table
  {
    dwarf_3_attributes ()
    {
      add (new block_or_loc_attribute (DW_AT_location));
      add (new block_const_ref_attribute (DW_AT_byte_size));
      add (new block_const_ref_attribute (DW_AT_bit_offset));
      add (new block_const_ref_attribute (DW_AT_bit_size));
      add (new basic_attribute (DW_AT_stmt_list, cl_lineptr));
      add (new block_or_loc_attribute (DW_AT_string_length));
      add (new basic_attribute (DW_AT_const_value,
				dw_class_set (cl_block, cl_constant,
					      cl_string)));
      add (new block_const_ref_attribute (DW_AT_lower_bound));
      add (new block_or_loc_attribute (DW_AT_return_addr));
      add (new const_attribute (DW_AT_bit_stride));
      add (new block_const_ref_attribute (DW_AT_upper_bound));
      add (new block_const_ref_attribute (DW_AT_count));
      add (new basic_attribute (DW_AT_data_member_location,
				dw_class_set (cl_block, cl_constant,
					      cl_loclistptr)));
      add (new block_or_loc_attribute (DW_AT_frame_base));
      add (new basic_attribute (DW_AT_macro_info, cl_macptr));
      add (new block_or_loc_attribute (DW_AT_segment));
      add (new block_or_loc_attribute (DW_AT_static_link));
      add (new block_or_loc_attribute (DW_AT_use_location));
      add (new block_or_loc_attribute (DW_AT_vtable_elem_location));
      add (new block_const_ref_attribute (DW_AT_associated));
      add (new block_attribute (DW_AT_data_location));
      add (new block_const_ref_attribute (DW_AT_byte_stride));
      add (new addr_attribute (DW_AT_entry_pc));
      add (new flag_attribute (DW_AT_use_UTF8));
      add (new ref_attribute (DW_AT_extension));
      add (new basic_attribute (DW_AT_ranges, cl_rangelistptr));
      add (new basic_attribute (DW_AT_trampoline,
				dw_class_set (cl_address, cl_flag,
					      cl_reference, cl_string)));
      add (new const_attribute (DW_AT_call_column));
      add (new const_attribute (DW_AT_call_file));
      add (new const_attribute (DW_AT_call_line));
      add (new string_attribute (DW_AT_description));
      add (new const_attribute (DW_AT_binary_scale));
      add (new const_attribute (DW_AT_decimal_scale));
      add (new ref_attribute (DW_AT_small));
      add (new const_attribute (DW_AT_decimal_sign));
      add (new const_attribute (DW_AT_digit_count));
      add (new string_attribute (DW_AT_picture_string));
      add (new flag_attribute (DW_AT_mutable));
      add (new flag_attribute (DW_AT_threads_scaled));
      add (new flag_attribute (DW_AT_explicit));
      add (new ref_attribute (DW_AT_object_pointer));
      add (new const_attribute (DW_AT_endianity));
      add (new flag_attribute (DW_AT_elemental));
      add (new flag_attribute (DW_AT_pure));
      add (new flag_attribute (DW_AT_recursive));
    }
  };

  typedef preset_form<sc_value,
		      cl_constant, cl_lineptr, cl_loclistptr,
		      cl_macptr, cl_rangelistptr> dw3_data_form;

  struct dwarf_3_forms
    : public form_table
  {
    dwarf_3_forms ()
    {
      add (new dw3_data_form (DW_FORM_data4, fw_4));
      add (new dw3_data_form (DW_FORM_data8, fw_8));
      add (new offset_form (DW_FORM_ref_addr, cl_reference));
    }
  };

  typedef preset_attribute<cl_exprloc, cl_loclistptr> exprloc_loclist_attribute;
  typedef preset_attribute<cl_constant, cl_exprloc, cl_reference>
  const_exprloc_ref_attribute;

  struct dwarf_4_attributes
    : public attribute_table
  {
    dwarf_4_attributes ()
    {
      add (new exprloc_loclist_attribute (DW_AT_location));
      add (new const_exprloc_ref_attribute (DW_AT_bit_offset));
      add (new const_exprloc_ref_attribute (DW_AT_bit_size));
      add (new basic_attribute (DW_AT_high_pc,
				dw_class_set (cl_address, cl_constant)));
      add (new exprloc_loclist_attribute (DW_AT_string_length));
      add (new basic_attribute (DW_AT_const_value,
				dw_class_set (cl_block, cl_constant,
					      cl_string)));
      add (new const_exprloc_ref_attribute (DW_AT_lower_bound));
      add (new exprloc_loclist_attribute (DW_AT_return_addr));
      add (new const_exprloc_ref_attribute (DW_AT_bit_stride));
      add (new const_exprloc_ref_attribute (DW_AT_upper_bound));
      add (new const_exprloc_ref_attribute (DW_AT_count));
      add (new basic_attribute (DW_AT_data_member_location,
				dw_class_set (cl_constant, cl_exprloc,
					      cl_loclistptr)));
      add (new exprloc_loclist_attribute (DW_AT_frame_base));
      add (new ref_attribute (DW_AT_namelist_item));
      add (new exprloc_loclist_attribute (DW_AT_segment));
      add (new exprloc_loclist_attribute (DW_AT_static_link));
      add (new exprloc_loclist_attribute (DW_AT_use_location));
      add (new exprloc_loclist_attribute (DW_AT_vtable_elem_location));
      add (new const_exprloc_ref_attribute (DW_AT_allocated));
      add (new const_exprloc_ref_attribute (DW_AT_associated));
      add (new basic_attribute (DW_AT_data_location, cl_exprloc));
      add (new const_exprloc_ref_attribute (DW_AT_byte_stride));
      add (new ref_attribute (DW_AT_signature));
      add (new flag_attribute (DW_AT_main_subprogram));
      add (new const_attribute (DW_AT_data_bit_offset));
      add (new flag_attribute (DW_AT_const_expr));
    }
  };

  struct exprloc_form
    : public preset_form<sc_block, cl_exprloc>
  {
    exprloc_form (int a_name)
      : preset_form<sc_block, cl_exprloc> (a_name, fw_uleb)
    {}
  };

  struct dwarf_4_forms
    : public form_table
  {
    dwarf_4_forms ()
    {
      add (new const_form (DW_FORM_data4, fw_4));
      add (new const_form (DW_FORM_data8, fw_8));
      add (new offset_form
	   (DW_FORM_sec_offset,
	    cl_lineptr, cl_loclistptr, cl_macptr, cl_rangelistptr));
      add (new exprloc_form (DW_FORM_exprloc));
      add (new flag_form (DW_FORM_flag_present, fw_0));
      add (new ref_form (DW_FORM_ref_sig8, fw_8));
    }
  };

  class std_dwarf
    : public dwarf_version
  {
    attribute_table const _m_attrtab;
    form_table const _m_formtab;
    dwarf_version const *_m_parent;

    template<class T>
    T const *
    lookfor (index_table<T> const &table, int name,
	     T const*(dwarf_version::*fail) (int) const) const
    {
      if (T const *emt = table.get (name))
	return emt;
      else if (_m_parent != NULL)
	return (_m_parent->*fail) (name);
      else
	return NULL;
    }

  public:
    std_dwarf (attribute_table const &attrtab,
	       form_table const &formtab,
	       dwarf_version const *parent = NULL)
      : _m_attrtab (attrtab)
      , _m_formtab (formtab)
      , _m_parent (parent)
    {}

    form const *
    get_form (int form_name) const
    {
      return lookfor (_m_formtab, form_name, &dwarf_version::get_form);
    }

    attribute const *
    get_attribute (int attribute_name) const
    {
      return lookfor (_m_attrtab, attribute_name,
		      &dwarf_version::get_attribute);
    }

    bool
    form_allowed (int attribute_name, int form_name) const
    {
      attribute const *attribute = this->get_attribute (attribute_name);
      assert (attribute != NULL);
      dw_class_set const &attr_classes = attribute->classes ();

      form const *form = this->get_form (form_name);
      assert (form != NULL);
      dw_class_set const &form_classes = form->classes ();

      return (attr_classes & form_classes).any ();
    }
  };

  std_dwarf dwarf2 (dwarf_2_attributes (), dwarf_2_forms (), NULL);
  std_dwarf dwarf3 (dwarf_3_attributes (), dwarf_3_forms (), &dwarf2);
  std_dwarf dwarf4 (dwarf_4_attributes (), dwarf_4_forms (), &dwarf3);
}

dwarf_version const *
dwarf_version::get (unsigned version)
{
  switch (version)
    {
    case 2: return &dwarf2;
    case 3: return &dwarf3;
    case 4: return &dwarf4;
    default: return NULL;
    };
}

dwarf_version const *
dwarf_version::get_latest ()
{
  return get (4);
}

sibling_form_suitable_t
sibling_form_suitable (dwarf_version const *ver, int form)
{
  if (!ver->form_allowed (DW_AT_sibling, form))
    return sfs_invalid;
  else if (form == DW_FORM_ref_addr)
    return sfs_long;
  else
    return sfs_ok;
}

bool
dwarf_version::form_allowed (int form) const
{
  return get_form (form) != NULL;
}

#if 0

.at (DW_AT_abstract_origin)
.version (dwarf_2, dwarf_3, dwarf_4).classes (reference)

.ad (DW_AT_accessibility)
.version (dwarf_2, dwarf_3, dwarf_4).classes (constant)

.at (DW_AT_allocated)
.version (dwarf_3).classes (constant, block, reference)
.version (dwarf_4).classes (constant, exprloc, reference)

;


{DW_AT_abstract_origin, 		0x31	2,3,4	reference

DW_AT_accessibility		0x32	2,3,4	constant

DW_AT_address_class		0x33	2,3,4	constant


// compositions of dwarf_version:
//  - extend (A, B): allow union of A and B
//  - strict (A, B): allow intersection of A and B

// AT->class
DW_AT_abstract_origin		0x31	/*2,3,4*/reference

DW_AT_accessibility		0x32	/*2,3,4*/constant

DW_AT_address_class		0x33	/*2,3,4*/constant

DW_AT_allocated			0x4e	/*3*/constant,block,reference
DW_AT_allocated			0x4e	/*4*/constant,exprloc,reference

DW_AT_artificial		0x34	/*2,3,4*/flag

DW_AT_associated		0x4f	/*3*/block,constant,reference
DW_AT_associated		0x4f	/*4*/constant,exprloc,reference

DW_AT_base_types		0x35	/*2,3,4*/reference

DW_AT_binary_scale		0x5b	/*3,4*/constant

DW_AT_bit_offset		0x0c	/*2*/constant
DW_AT_bit_offset		0x0c	/*3*/block,constant,reference
DW_AT_bit_offset		0x0c	/*4*/constant,exprloc,reference

DW_AT_bit_size			0x0d	/*2*/constant
DW_AT_bit_size			0x0d	/*3*/block,constant,reference
DW_AT_bit_size			0x0d	/*4*/constant,exprloc,reference

DW_AT_bit_stride		0x2e	/*3*/constant
DW_AT_bit_stride		0x2e	/*4*/constant,exprloc,reference

DW_AT_byte_size			0x0b	/*2*/constant
DW_AT_byte_size			0x0b	/*3*/block,constant,reference
DW_AT_byte_size			0x0b	/*4*/constant,exprloc,reference

DW_AT_byte_stride		0x51	/*3*/block,constant,reference
DW_AT_byte_stride		0x51	/*4*/constant,exprloc,reference

DW_AT_call_column		0x57	/*3,4*/constant

DW_AT_call_file			0x58	/*3,4*/constant

DW_AT_call_line			0x59	/*3,4*/constant

DW_AT_calling_convention	0x36	/*2,3,4*/constant

DW_AT_common_reference		0x1a	/*2,3,4*/reference

DW_AT_comp_dir			0x1b	/*2,3,4*/string

DW_AT_const_expr		0x6c	/*4*/flag

DW_AT_const_value		0x1c	/*2*/string,constant,block
DW_AT_const_value		0x1c	/*3*/block,constant,string
DW_AT_const_value		0x1c	/*4*/block,constant,string

DW_AT_containing_type		0x1d	/*2,3,4*/reference

DW_AT_count			0x37	/*2*/constant,reference
DW_AT_count			0x37	/*3*/block,constant,reference
DW_AT_count			0x37	/*4*/constant,exprloc,reference

DW_AT_data_bit_offset		0x6b	/*4*/constant

DW_AT_data_location		0x50	/*3*/block
DW_AT_data_location		0x50	/*4*/exprloc

DW_AT_data_member_location	0x38	/*2*/block,reference
DW_AT_data_member_location	0x38	/*3*/block,constant,loclistptr
DW_AT_data_member_location	0x38	/*4*/constant,exprloc,loclistptr

DW_AT_decimal_scale		0x5c	/*3,4*/constant

DW_AT_decimal_sign		0x5e	/*3,4*/constant

DW_AT_decl_column		0x39	/*2,3,4*/constant

DW_AT_decl_file			0x3a	/*2,3,4*/constant

DW_AT_decl_line			0x3b	/*2,3,4*/constant

DW_AT_declaration		0x3c	/*2,3,4*/flag

DW_AT_default_value		0x1e	/*2,3,4*/reference

DW_AT_description		0x5a	/*3,4*/string

DW_AT_digit_count		0x5f	/*3,4*/constant

DW_AT_discr			0x15	/*2,3,4*/reference

DW_AT_discr_list		0x3d	/*2,3,4*/block

DW_AT_discr_value		0x16	/*2,3,4*/constant

DW_AT_elemental			0x66	/*3,4*/flag

DW_AT_encoding			0x3e	/*2,3,4*/constant

DW_AT_endianity			0x65	/*3,4*/constant

DW_AT_entry_pc			0x52	/*3,4*/address

DW_AT_explicit			0x63	/*3,4*/flag

DW_AT_extension			0x54	/*3,4*/reference

DW_AT_external			0x3f	/*2,3,4*/flag

DW_AT_frame_base		0x40	/*2*/block,constant
DW_AT_frame_base		0x40	/*3*/block,loclistptr
DW_AT_frame_base		0x40	/*4*/exprloc,loclistptr

DW_AT_friend			0x41	/*2,3,4*/reference

DW_AT_high_pc			0x12	/*2,3*/address
DW_AT_high_pc			0x12	/*4*/address,constant

DW_AT_identifier_case		0x42	/*2,3,4*/constant

DW_AT_import			0x18	/*2,3,4*/reference

DW_AT_inline			0x20	/*2,3,4*/constant

DW_AT_is_optional		0x21	/*2,3,4*/flag

DW_AT_language			0x13	/*2,3,4*/constant

DW_AT_location			0x02	/*2*/block,constant
DW_AT_location			0x02	/*3*/block,loclistptr
DW_AT_location			0x02	/*4*/exprloc,loclistptr

DW_AT_low_pc			0x11	/*2,3,4*/address

DW_AT_lower_bound		0x22	/*2*/constant,reference
DW_AT_lower_bound		0x22	/*3*/block,constant,reference
DW_AT_lower_bound		0x22	/*4*/constant,exprloc,reference

DW_AT_macro_info		0x43	/*2*/constant
DW_AT_macro_info		0x43	/*3,4*/macptr

DW_AT_main_subprogram		0x6a	/*4*/flag

DW_AT_mutable			0x61	/*3,4*/flag

DW_AT_name			0x03	/*2,3,4*/string

DW_AT_namelist_item		0x44	/*2,3*/block
DW_AT_namelist_item		0x44	/*4*/reference

DW_AT_object_pointer		0x64	/*3,4*/reference

DW_AT_ordering			0x09	/*2,3,4*/constant

DW_AT_picture_string		0x60	/*3,4*/string

DW_AT_priority			0x45	/*2,3,4*/reference

DW_AT_producer			0x25	/*2,3,4*/string

DW_AT_prototyped		0x27	/*2,3,4*/flag

DW_AT_pure			0x67	/*3,4*/flag

DW_AT_ranges			0x55	/*3,4*/rangelistptr

DW_AT_recursive			0x68	/*3,4*/flag

DW_AT_return_addr		0x2a	/*2*/block,constant
DW_AT_return_addr		0x2a	/*3*/block,loclistptr
DW_AT_return_addr		0x2a	/*4*/exprloc,loclistptr

DW_AT_segment			0x46	/*2*/block,constant
DW_AT_segment			0x46	/*3*/block,loclistptr
DW_AT_segment			0x46	/*4*/exprloc,loclistptr

DW_AT_sibling			0x01	/*2,3,4*/reference

DW_AT_signature			0x69	/*4*/reference

DW_AT_small			0x5d	/*3,4*/reference

DW_AT_specification		0x47	/*2,3,4*/reference

DW_AT_start_scope		0x2c	/*2,3,4*/constant

DW_AT_static_link		0x48	/*2*/block,constant
DW_AT_static_link		0x48	/*3*/block,loclistptr
DW_AT_static_link		0x48	/*4*/exprloc,loclistptr

DW_AT_stmt_list			0x10	/*2*/constant
DW_AT_stmt_list			0x10	/*3,4*/lineptr

DW_AT_stride_size		0x2e	/*2*/constant

DW_AT_string_length		0x19	/*2*/block,constant
DW_AT_string_length		0x19	/*3*/block,loclistptr
DW_AT_string_length		0x19	/*4*/exprloc,loclistptr

DW_AT_threads_scaled		0x62	/*3,4*/flag

DW_AT_trampoline		0x56	/*3,4*/address,flag,reference,string

DW_AT_type			0x49	/*2,3,4*/reference

DW_AT_upper_bound		0x2f	/*2*/constant
DW_AT_upper_bound		0x2f	/*3*/block,constant,reference
DW_AT_upper_bound		0x2f	/*4*/constant,exprloc,reference

DW_AT_use_UTF8			0x53	/*3,4*/flag

DW_AT_use_location		0x4a	/*2*/block,constant
DW_AT_use_location		0x4a	/*3*/block,loclistptr
DW_AT_use_location		0x4a	/*4*/exprloc,loclistptr

DW_AT_variable_parameter	0x4b	/*2,3,4*/flag

DW_AT_virtuality		0x4c	/*2,3,4*/constant

DW_AT_visibility		0x17	/*2,3,4*/constant

DW_AT_vtable_elem_location	0x4d	/*2*/block,reference
DW_AT_vtable_elem_location	0x4d	/*3*/block,loclistptr
DW_AT_vtable_elem_location	0x4d	/*4*/exprloc,loclistptr


// FORM->class
DW_FORM_addr		0x01	/*2,3,4*/address

DW_FORM_block		0x09	/*2,3,4*/block

DW_FORM_block1		0x0a	/*2,3,4*/block

DW_FORM_block2		0x03	/*2,3,4*/block

DW_FORM_block4		0x04	/*2,3,4*/block

DW_FORM_data1		0x0b	/*2,3,4*/constant

DW_FORM_data2		0x05	/*2,3,4*/constant

DW_FORM_data4		0x06	/*2,4*/constant
DW_FORM_data4		0x06	/*3*/constant, lineptr, loclistptr, macptr, rangelistptr

DW_FORM_data8		0x07	/*2,4*/constant
DW_FORM_data8		0x07	/*3*/constant, lineptr, loclistptr, macptr, rangelistptr

DW_FORM_exprloc		0x18	/*4*/exprloc

DW_FORM_flag		0x0c	/*2,3,4*/flag

DW_FORM_flag_present	0x19	/*4*/flag

DW_FORM_indirect	0x16	/*2,3,4*/-

DW_FORM_ref1		0x11	/*2,3,4*/reference

DW_FORM_ref2		0x12	/*2,3,4*/reference

DW_FORM_ref4		0x13	/*2,3,4*/reference

DW_FORM_ref8		0x14	/*2,3,4*/reference

DW_FORM_ref_addr	0x10	/*2,3,4*/reference

DW_FORM_ref_sig8	0x20	/*4*/reference

DW_FORM_ref_udata	0x15	/*2,3,4*/reference

DW_FORM_sdata		0x0d	/*2,3,4*/constant

DW_FORM_sec_offset	0x17	/*4*/lineptr, loclistptr, macptr, rangelistptr

DW_FORM_string		0x08	/*2,3,4*/string

DW_FORM_strp		0x0e	/*2,3,4*/string

DW_FORM_udata		0x0f	/*2,3,4*/constant

#endif
