/* Pedantic checking of DWARF files.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2009.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <iostream>
#include <set>
#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cassert>
#include <iterator>

#include "dwarflint.h"
#include "dwarfstrings.h"
#include "c++/dwarf"
#include "../libdw/libdwP.h"

namespace
{
  message_category cat (message_category c1,
			message_category c2,
			message_category c3 = mc_none)
  {
    return static_cast<message_category> (c1 | c2 | c3);
  }

  template <class T>
  std::string
  to_string (T x)
  {
    std::ostringstream o;
    o << x;
    return o.str();
  }
}

struct hl_ctx
{
  elfutils::dwarf dw;

  hl_ctx (Dwarf *dwarf)
    : dw (dwarf)
  {
  }
};

hl_ctx *
hl_ctx_new (Dwarf *dwarf)
{
  return new hl_ctx (dwarf);
}

void
hl_ctx_delete (hl_ctx *hlctx)
{
  delete hlctx;
}

class acceptable_form
{
  typedef std::set <int> form_set;
  form_set m_forms;

public:
  acceptable_form () {}

  acceptable_form (int f, ...)
  {
    va_list ap;
    va_start (ap, f);
    m_forms.insert (f);
    while ((f = va_arg (ap, int)))
      m_forms.insert (f);
    va_end (ap);
  }

  acceptable_form operator+ (acceptable_form const &other) const
  {
    acceptable_form ret;
    ret.m_forms.insert (m_forms.begin (), m_forms.end ());
    ret.m_forms.insert (other.m_forms.begin (), other.m_forms.end ());
    return ret;
  }

  virtual bool acceptable (int form) const
  {
    return m_forms.find (form) != m_forms.end ();
  }
};

class acceptable_form_map
{
  typedef std::map <int, acceptable_form> acceptable_map;
  acceptable_map m_map;

public:
  acceptable_form_map ()
  {
    acceptable_form form_c_address (DW_FORM_addr, 0);
    acceptable_form form_c_block (DW_FORM_block1, DW_FORM_block2,
				  DW_FORM_block4, DW_FORM_block, 0);
    acceptable_form form_c_constant (DW_FORM_data1, DW_FORM_data2,
				     DW_FORM_data4, DW_FORM_data8,
				     DW_FORM_sdata, DW_FORM_udata, 0);
    acceptable_form form_c_flag (DW_FORM_flag, 0);
    acceptable_form form_c_ptr (DW_FORM_data4, DW_FORM_data8, 0);
    // ^^ lineptr, loclistptr, macptr, rangelistptr
    acceptable_form form_c_reference (DW_FORM_ref1, DW_FORM_ref2,
				      DW_FORM_ref4, DW_FORM_ref8,
				      DW_FORM_ref_udata, DW_FORM_ref_addr, 0);
    acceptable_form form_c_string (DW_FORM_strp, DW_FORM_string, 0);
    acceptable_form form_c_bcr = form_c_block + form_c_constant + form_c_reference;

    std::map <int, acceptable_form> acceptable_forms;
    m_map[DW_AT_sibling] = form_c_reference;
    m_map[DW_AT_location] = form_c_block + form_c_ptr;
    m_map[DW_AT_name] = form_c_string;
    m_map[DW_AT_ordering] = form_c_constant;
    m_map[DW_AT_byte_size] = form_c_bcr;
    m_map[DW_AT_bit_offset] = form_c_bcr;
    m_map[DW_AT_bit_size] = form_c_bcr;
    m_map[DW_AT_stmt_list] = form_c_ptr;
    m_map[DW_AT_low_pc] = form_c_address;
    m_map[DW_AT_high_pc] = form_c_address;
    m_map[DW_AT_language] = form_c_constant;
    m_map[DW_AT_discr] = form_c_reference;
    m_map[DW_AT_discr_value] = form_c_constant;
    m_map[DW_AT_visibility] = form_c_constant;
    m_map[DW_AT_import] = form_c_reference;
    m_map[DW_AT_string_length] = form_c_block + form_c_ptr;
    m_map[DW_AT_common_reference] = form_c_reference;
    m_map[DW_AT_comp_dir] = form_c_string;
    m_map[DW_AT_const_value] = form_c_block + form_c_constant + form_c_string;
    m_map[DW_AT_containing_type] = form_c_reference;
    m_map[DW_AT_default_value] = form_c_reference;
    m_map[DW_AT_inline] = form_c_constant;
    m_map[DW_AT_is_optional] = form_c_flag;
    m_map[DW_AT_lower_bound] = form_c_block + form_c_constant + form_c_reference;
    m_map[DW_AT_producer] = form_c_string;
    m_map[DW_AT_prototyped] = form_c_flag;
    m_map[DW_AT_return_addr] = form_c_block + form_c_ptr;
    m_map[DW_AT_start_scope] = form_c_constant;
    m_map[DW_AT_bit_stride] = form_c_constant;
    m_map[DW_AT_upper_bound] = form_c_bcr;
    m_map[DW_AT_abstract_origin] = form_c_reference;
    m_map[DW_AT_accessibility] = form_c_constant;
    m_map[DW_AT_address_class] = form_c_constant;
    m_map[DW_AT_artificial] = form_c_flag;
    m_map[DW_AT_base_types] = form_c_reference;
    m_map[DW_AT_calling_convention] = form_c_constant;
    m_map[DW_AT_count] = form_c_bcr;
    m_map[DW_AT_data_member_location] = form_c_block + form_c_constant + form_c_ptr;
    m_map[DW_AT_decl_column] = form_c_constant;
    m_map[DW_AT_decl_file] = form_c_constant;
    m_map[DW_AT_decl_line] = form_c_constant;
    m_map[DW_AT_declaration] = form_c_flag;
    m_map[DW_AT_discr_list] = form_c_block;
    m_map[DW_AT_encoding] = form_c_constant;
    m_map[DW_AT_external] = form_c_flag;
    m_map[DW_AT_frame_base] = form_c_block + form_c_ptr;
    m_map[DW_AT_friend] = form_c_reference;
    m_map[DW_AT_identifier_case] = form_c_constant;
    m_map[DW_AT_macro_info] = form_c_ptr;
    m_map[DW_AT_namelist_item] = form_c_block;
    m_map[DW_AT_priority] = form_c_reference;
    m_map[DW_AT_segment] = form_c_block + form_c_ptr;
    m_map[DW_AT_specification] = form_c_reference;
    m_map[DW_AT_static_link] = form_c_block + form_c_ptr;
    m_map[DW_AT_type] = form_c_reference;
    m_map[DW_AT_use_location] = form_c_block + form_c_ptr;
    m_map[DW_AT_variable_parameter] = form_c_flag;
    m_map[DW_AT_virtuality] = form_c_constant;
    m_map[DW_AT_vtable_elem_location] = form_c_block + form_c_ptr;
    m_map[DW_AT_allocated] = form_c_bcr;
    m_map[DW_AT_associated] = form_c_bcr;
    m_map[DW_AT_data_location] = form_c_block;
    m_map[DW_AT_byte_stride] = form_c_bcr;
    m_map[DW_AT_entry_pc] = form_c_address;
    m_map[DW_AT_use_UTF8] = form_c_flag;
    m_map[DW_AT_extension] = form_c_reference;
    m_map[DW_AT_ranges] = form_c_ptr;
    m_map[DW_AT_trampoline] = form_c_address + form_c_flag + form_c_reference + form_c_string;
    m_map[DW_AT_call_column] = form_c_constant;
    m_map[DW_AT_call_file] = form_c_constant;
    m_map[DW_AT_call_line] = form_c_constant;
    m_map[DW_AT_description] = form_c_string;
    m_map[DW_AT_binary_scale] = form_c_constant;
    m_map[DW_AT_decimal_scale] = form_c_constant;
    m_map[DW_AT_small] = form_c_reference;
    m_map[DW_AT_decimal_sign] = form_c_constant;
    m_map[DW_AT_digit_count] = form_c_constant;
    m_map[DW_AT_picture_string] = form_c_string;
    m_map[DW_AT_mutable] = form_c_flag;
    m_map[DW_AT_threads_scaled] = form_c_flag;
    m_map[DW_AT_explicit] = form_c_flag;
    m_map[DW_AT_object_pointer] = form_c_reference;
    m_map[DW_AT_endianity] = form_c_constant;
    m_map[DW_AT_elemental] = form_c_flag;
    m_map[DW_AT_pure] = form_c_flag;
    m_map[DW_AT_recursive] = form_c_flag;
  }

  bool acceptable (int attribute, int form) const
  {
    acceptable_map::const_iterator it = m_map.find (attribute);
    if (it != m_map.end ())
      throw std::runtime_error ("Unknown attribute #" + ::to_string (attribute));
    else
      return it->second.acceptable (form);
  }
};

enum optionality
{
  opt_optional = 0,	// may or may not be present
  opt_required,		// bogus if missing
  opt_expected,		// suspicious if missing
};

struct expected_set
{
  typedef std::map <int, optionality> expectation_map;

private:
  expectation_map m_map;

public:
#define DEF_FILLER(WHAT)						\
  expected_set &WHAT (int attribute)					\
  {									\
    assert (m_map.find (attribute) == m_map.end ());			\
    m_map.insert (std::make_pair (attribute, opt_##WHAT));		\
    return *this;							\
  }									\
  expected_set &WHAT (std::set <int> const &attributes)			\
  {									\
    for (std::set <int>::const_iterator it = attributes.begin ();	\
	 it != attributes.end (); ++it)					\
      WHAT (*it);							\
    return *this;							\
  }

  DEF_FILLER (required)
  DEF_FILLER (expected)
  DEF_FILLER (optional)
#undef DEF_FILLER

  expectation_map const &map () const
  {
    return m_map;
  }
};

class expected_map
{
  typedef std::map <int, expected_set> expected_map_t;

protected:
  expected_map_t m_map;
  expected_map () {}

public:
  expected_set::expectation_map const &map (int tag) const
  {
    expected_map_t::const_iterator it = m_map.find (tag);
    if (it == m_map.end ())
      throw std::runtime_error ("Unknown tag #" + ::to_string (tag));
    return it->second.map ();
  }
};

struct expected_at_map
  : public expected_map
{
  expected_at_map ()
  {
    std::set <int> at_set_decl;
    at_set_decl.insert (DW_AT_decl_column);
    at_set_decl.insert (DW_AT_decl_file);
    at_set_decl.insert (DW_AT_decl_line);

    m_map [DW_TAG_access_declaration]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      ;

    m_map[DW_TAG_array_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_bit_stride)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_ordering)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_base_type]
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_binary_scale)
      .optional (DW_AT_bit_offset)
      .optional (DW_AT_bit_size)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_decimal_scale)
      .optional (DW_AT_decimal_sign)
      .optional (DW_AT_description)
      .optional (DW_AT_digit_count)
      .optional (DW_AT_encoding)
      .optional (DW_AT_endianity)
      .optional (DW_AT_name)
      .optional (DW_AT_picture_string)
      .optional (DW_AT_sibling)
      .optional (DW_AT_small)
      ;

    m_map [DW_TAG_catch_block]
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_class_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_visibility)
      .optional (DW_AT_containing_type) // XXX added to reflect reality
      ;

    m_map [DW_TAG_common_block]
      .optional (at_set_decl)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_location)
      .optional (DW_AT_name)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_common_inclusion]
      .optional (at_set_decl)
      .optional (DW_AT_common_reference)
      .optional (DW_AT_declaration)
      .optional (DW_AT_sibling)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_compile_unit]
      .optional (DW_AT_base_types)
      .optional (DW_AT_comp_dir)
      .optional (DW_AT_identifier_case)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_language)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_macro_info)
      .optional (DW_AT_name)
      .optional (DW_AT_producer)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_stmt_list)
      .optional (DW_AT_use_UTF8)
      .optional (DW_AT_entry_pc) // XXX added to reflect reality
      ;

    m_map [DW_TAG_condition]
      .optional (at_set_decl)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_const_type]
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_constant]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_const_value)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_endianity)
      .optional (DW_AT_external)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_dwarf_procedure]
      .optional (DW_AT_location)
      ;

    m_map [DW_TAG_entry_point]
      .optional (DW_AT_address_class)
      .optional (DW_AT_description)
      .optional (DW_AT_frame_base)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_name)
      .optional (DW_AT_return_addr)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_static_link)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_enumeration_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_bit_stride)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_byte_stride)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_enumerator]
      .optional (at_set_decl)
      .optional (DW_AT_const_value)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_file_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_formal_parameter]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_artificial)
      .optional (DW_AT_const_value)
      .optional (DW_AT_default_value)
      .optional (DW_AT_description)
      .optional (DW_AT_endianity)
      .optional (DW_AT_is_optional)
      .optional (DW_AT_location)
      .optional (DW_AT_name)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_variable_parameter)
      ;

    m_map [DW_TAG_friend]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_friend)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_imported_declaration]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_description)
      .optional (DW_AT_import)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      ;

    m_map [DW_TAG_imported_module]
      .optional (at_set_decl)
      .optional (DW_AT_import)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      ;

    m_map [DW_TAG_imported_unit]
      .optional (DW_AT_import)
      ;

    m_map [DW_TAG_inheritance]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_data_member_location)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_virtuality)
      ;

    m_map [DW_TAG_inlined_subroutine]
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_call_column)
      .optional (DW_AT_call_file)
      .optional (DW_AT_call_line)
      .optional (DW_AT_entry_pc)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_ranges)
      .optional (DW_AT_return_addr)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_trampoline)
      ;

    m_map [DW_TAG_interface_type]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      ;

    m_map [DW_TAG_label]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_description)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_name)
      .optional (DW_AT_segment)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_lexical_block]
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_description)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_name)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_member]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_bit_offset)
      .optional (DW_AT_bit_size)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_member_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_mutable)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_module]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_entry_pc)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_name)
      .optional (DW_AT_priority)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_namelist]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_declaration)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_namelist_item]
      .optional (at_set_decl)
      .optional (DW_AT_namelist_item)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_namespace]
      .optional (at_set_decl)
      .optional (DW_AT_description)
      .optional (DW_AT_extension)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      ;

    m_map [DW_TAG_packed_type]
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_partial_unit]
      .optional (DW_AT_base_types)
      .optional (DW_AT_comp_dir)
      .optional (DW_AT_description)
      .optional (DW_AT_identifier_case)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_language)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_macro_info)
      .optional (DW_AT_name)
      .optional (DW_AT_producer)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_stmt_list)
      .optional (DW_AT_use_UTF8)
      ;

    m_map [DW_TAG_pointer_type]
      .optional (DW_AT_address_class)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_type)
      .optional (DW_AT_byte_size) // XXX added to reflect reality
      ;

    m_map [DW_TAG_ptr_to_member_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_address_class)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_containing_type)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_use_location)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_reference_type]
      .optional (DW_AT_address_class)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_byte_size) // XXX added to reflect reality
      ;

    m_map [DW_TAG_restrict_type]
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_set_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_shared_type]
      .optional (at_set_decl)
      .optional (DW_AT_count)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_string_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_string_length)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_structure_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_visibility)
      .optional (DW_AT_containing_type) // XXX added to reflect reality
      ;

    m_map [DW_TAG_subprogram]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_address_class)
      .optional (DW_AT_artificial)
      .optional (DW_AT_calling_convention)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_elemental)
      .optional (DW_AT_entry_pc)
      .optional (DW_AT_explicit)
      .optional (DW_AT_external)
      .optional (DW_AT_frame_base)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_inline)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_name)
      .optional (DW_AT_object_pointer)
      .optional (DW_AT_prototyped)
      .optional (DW_AT_pure)
      .optional (DW_AT_ranges)
      .optional (DW_AT_recursive)
      .optional (DW_AT_return_addr)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_static_link)
      .optional (DW_AT_trampoline)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      .optional (DW_AT_virtuality)
      .optional (DW_AT_vtable_elem_location)
      .optional (DW_AT_MIPS_linkage_name) // XXX added to reflect reality
      .optional (DW_AT_containing_type) // XXX added to reflect reality
      ;

    m_map [DW_TAG_subrange_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_bit_stride)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_byte_stride)
      .optional (DW_AT_count)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_lower_bound)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_threads_scaled)
      .optional (DW_AT_type)
      .optional (DW_AT_upper_bound)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_subroutine_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_address_class)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_prototyped)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_template_type_parameter]
      .optional (at_set_decl)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_template_value_parameter ]
      .optional (at_set_decl)
      .optional (DW_AT_const_value)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_thrown_type]
      .optional (at_set_decl)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_try_block]
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_typedef]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_union_type]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_byte_size)
      .optional (DW_AT_data_location)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_visibility)
      ;

    m_map [DW_TAG_unspecified_parameters]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_artificial)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_unspecified_type]
      .optional (at_set_decl)
      .optional (DW_AT_description)
      .optional (DW_AT_name)
      ;

    m_map [DW_TAG_variable]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_const_value)
      .optional (DW_AT_declaration)
      .optional (DW_AT_description)
      .optional (DW_AT_endianity)
      .optional (DW_AT_external)
      .optional (DW_AT_location)
      .optional (DW_AT_name)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_specification)
      .optional (DW_AT_start_scope)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      .optional (DW_AT_MIPS_linkage_name) // XXX added to reflect reality
      .optional (DW_AT_artificial) // XXX added to reflect reality
      ;

    m_map [DW_TAG_variant]
      .optional (at_set_decl)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_declaration)
      .optional (DW_AT_discr_list)
      .optional (DW_AT_discr_value)
      .optional (DW_AT_sibling)
      ;

    m_map [DW_TAG_variant_part]
      .optional (at_set_decl)
      .optional (DW_AT_abstract_origin)
      .optional (DW_AT_accessibility)
      .optional (DW_AT_declaration)
      .optional (DW_AT_discr)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_volatile_type]
      .optional (at_set_decl)
      .optional (DW_AT_allocated)
      .optional (DW_AT_associated)
      .optional (DW_AT_data_location)
      .optional (DW_AT_name)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      ;

    m_map [DW_TAG_with_stmt]
      .optional (DW_AT_accessibility)
      .optional (DW_AT_address_class)
      .optional (DW_AT_declaration)
      .optional (DW_AT_high_pc)
      .optional (DW_AT_location)
      .optional (DW_AT_low_pc)
      .optional (DW_AT_ranges)
      .optional (DW_AT_segment)
      .optional (DW_AT_sibling)
      .optional (DW_AT_type)
      .optional (DW_AT_visibility)
      ;
  }
};

static const acceptable_form_map acceptable_form;
static const expected_at_map expected_at;
//static const expected_children_map expected_children;

bool
check_matching_ranges (hl_ctx *hlctx)
{
  struct where where_ref = WHERE (sec_info, NULL);
  struct where where_ar = WHERE (sec_aranges, NULL);
  where_ar.ref = &where_ref;
  struct where where_r = WHERE (sec_ranges, NULL);
  where_r.ref = &where_ref;

  const elfutils::dwarf::aranges_map &aranges = hlctx->dw.aranges ();
  for (elfutils::dwarf::aranges_map::const_iterator i = aranges.begin ();
       i != aranges.end (); ++i)
    {
      const elfutils::dwarf::compile_unit &cu = i->first;
      where_reset_1 (&where_ref, 0);
      where_reset_2 (&where_ref, cu.offset ());

      std::set<elfutils::dwarf::ranges::key_type>
	cu_aranges = i->second,
	cu_ranges = cu.ranges ();

      typedef std::vector <elfutils::dwarf::arange_list::value_type> range_vec;
      range_vec missing;
      std::back_insert_iterator <range_vec> i_missing (missing);

      std::set_difference (cu_aranges.begin (), cu_aranges.end (),
			   cu_ranges.begin (), cu_ranges.end (),
			   i_missing);

      for (range_vec::iterator it = missing.begin ();
	   it != missing.end (); ++it)
	wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_r,
		    ": missing range %#" PRIx64 "..%#" PRIx64
		    ", present in .debug_aranges.\n",
		    it->first, it->second);

      missing.clear ();
      std::set_difference (cu_ranges.begin (), cu_ranges.end (),
			   cu_aranges.begin (), cu_aranges.end (),
			   i_missing);

      for (range_vec::iterator it = missing.begin ();
	   it != missing.end (); ++it)
	wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_ar,
		    ": missing range %#" PRIx64 "..%#" PRIx64
		    ", present in .debug_ranges.\n",
		    it->first, it->second);
    }

  return true;
}

struct extract_tag {
  int operator () (elfutils::dwarf::attribute const &at) {
    return at.first;
  }
};

static void
recursively_validate (elfutils::dwarf::compile_unit const &cu,
		      elfutils::dwarf::debug_info_entry const &parent)
{
  struct where where = WHERE (sec_info, NULL);
  where_reset_1 (&where, cu.offset ());
  where_reset_2 (&where, parent.offset ());

  int parent_tag = parent.tag ();

  // Set of attributes of this DIE.
  std::set <int> attributes;
  std::transform (parent.attributes ().begin (),
		  parent.attributes ().end (),
		  std::inserter (attributes, attributes.end ()),
		  extract_tag ());

  // Attributes that we expect at this DIE.
  expected_set::expectation_map const &expect
    = expected_at.map (parent_tag);

  // Check missing attributes.
  for (expected_set::expectation_map::const_iterator jt
	 = expect.begin (); jt != expect.end (); ++jt)
    {
      std::set <int>::iterator kt = attributes.find (jt->first);
      if (kt == attributes.end ())
	switch (jt->second)
	  {
	  case opt_required:
	    wr_message (cat (mc_impact_4, mc_info), &where,
			": %s lacks required attribute %s.\n",
			dwarf_tag_string (parent_tag),
			dwarf_attr_string (jt->first));
	    break;

	  case opt_expected:
	    wr_message (cat (mc_impact_2, mc_info), &where,
			": %s should contain attribute %s.\n",
			dwarf_tag_string (parent_tag),
			dwarf_attr_string (jt->first));
	  case opt_optional:
	    break;
	  };
    }

  // Check unexpected attributes.
  for (std::set <int>::iterator jt = attributes.begin ();
       jt != attributes.end (); ++jt)
    {
      expected_set::expectation_map::const_iterator kt = expect.find (*jt);
      if (kt == expect.end ())
	wr_message (cat (mc_impact_3, mc_info), &where,
		    ": %s has attribute %s, which is not expected.\n",
		    dwarf_tag_string (parent_tag),
		    dwarf_attr_string (*jt));
    }

  // Check children recursively.
  class elfutils::dwarf::debug_info_entry::children const &children
    = parent.children ();
  for (elfutils::dwarf::debug_info_entry::children::const_iterator jt
	 = children.begin (); jt != children.end (); ++jt)
    recursively_validate (cu, *jt);
}

bool
check_expected_trees (hl_ctx *hlctx)
{
  class elfutils::dwarf::compile_units const &cus = hlctx->dw.compile_units ();
  for (elfutils::dwarf::compile_units::const_iterator it = cus.begin ();
       it != cus.end (); ++it)
    recursively_validate (*it, *it);

  return true;
}
