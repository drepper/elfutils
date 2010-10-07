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
#include "dwarf_2.hh"
#include "dwarf_3.hh"
#include "../libdw/dwarf.h"
#include <cassert>

namespace
{
  typedef preset_attribute<cl_block, cl_loclistptr> block_or_loc_attribute;
  typedef preset_attribute<cl_block, cl_constant, cl_reference>
  block_const_ref_attribute;

  struct dwarf_3_attributes
    : public attribute_table
  {
    dwarf_3_attributes ()
    {
      add (block_or_loc_attribute (DW_AT_location));
      add (block_const_ref_attribute (DW_AT_byte_size));
      add (block_const_ref_attribute (DW_AT_bit_offset));
      add (block_const_ref_attribute (DW_AT_bit_size));
      add (attribute (DW_AT_stmt_list, cl_lineptr));
      add (block_or_loc_attribute (DW_AT_string_length));
      add (attribute (DW_AT_const_value,
		      dw_class_set (cl_block, cl_constant, cl_string)));
      add (block_const_ref_attribute (DW_AT_lower_bound));
      add (block_or_loc_attribute (DW_AT_return_addr));
      add (const_attribute (DW_AT_bit_stride));
      add (block_const_ref_attribute (DW_AT_upper_bound));
      add (block_const_ref_attribute (DW_AT_count));
      add (attribute (DW_AT_data_member_location,
		      dw_class_set (cl_block, cl_constant, cl_loclistptr)));
      add (block_or_loc_attribute (DW_AT_frame_base));
      add (attribute (DW_AT_macro_info, cl_macptr));
      add (block_or_loc_attribute (DW_AT_segment));
      add (block_or_loc_attribute (DW_AT_static_link));
      add (block_or_loc_attribute (DW_AT_use_location));
      add (block_or_loc_attribute (DW_AT_vtable_elem_location));
      add (block_const_ref_attribute (DW_AT_associated));
      add (block_attribute (DW_AT_data_location));
      add (block_const_ref_attribute (DW_AT_byte_stride));
      add (addr_attribute (DW_AT_entry_pc));
      add (flag_attribute (DW_AT_use_UTF8));
      add (ref_attribute (DW_AT_extension));
      add (attribute (DW_AT_ranges, cl_rangelistptr));
      add (attribute (DW_AT_trampoline,
		      dw_class_set (cl_address, cl_flag,
				    cl_reference, cl_string)));
      add (const_attribute (DW_AT_call_column));
      add (const_attribute (DW_AT_call_file));
      add (const_attribute (DW_AT_call_line));
      add (string_attribute (DW_AT_description));
      add (const_attribute (DW_AT_binary_scale));
      add (const_attribute (DW_AT_decimal_scale));
      add (ref_attribute (DW_AT_small));
      add (const_attribute (DW_AT_decimal_sign));
      add (const_attribute (DW_AT_digit_count));
      add (string_attribute (DW_AT_picture_string));
      add (flag_attribute (DW_AT_mutable));
      add (flag_attribute (DW_AT_threads_scaled));
      add (flag_attribute (DW_AT_explicit));
      add (ref_attribute (DW_AT_object_pointer));
      add (const_attribute (DW_AT_endianity));
      add (flag_attribute (DW_AT_elemental));
      add (flag_attribute (DW_AT_pure));
      add (flag_attribute (DW_AT_recursive));
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
      add (dw3_data_form (DW_FORM_data4, fw_4));
      add (dw3_data_form (DW_FORM_data8, fw_8));
      add (offset_form (DW_FORM_ref_addr, cl_reference));
    }
  };

  struct dwarf_3_ext_t
    : public std_dwarf
  {
    dwarf_3_ext_t ()
      : std_dwarf (dwarf_3_attributes (), dwarf_3_forms ())
    {}

    dw_class
    ambiguous_class (__attribute__ ((unused)) form const *form,
		     attribute const *attribute,
		     dw_class_set const &candidates) const
    {
      assert (attribute->name () == DW_AT_data_member_location);
      assert (candidates == dw_class_set (cl_constant, cl_loclistptr));
      return cl_loclistptr;
    }
  };
}

dwarf_version const *
dwarf_3_ext ()
{
  static dwarf_3_ext_t dw;
  return &dw;
}

dwarf_version const *
dwarf_3 ()
{
  static dwarf_version const *dw =
    dwarf_version::extend (dwarf_2 (), dwarf_3_ext ());
  return dw;
}
