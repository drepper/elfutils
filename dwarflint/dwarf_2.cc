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
#include "../libdw/dwarf.h"

namespace
{
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

  struct dwarf_2_t
    : public std_dwarf
  {
    dwarf_2_t ()
      : std_dwarf (dwarf_2_attributes (), dwarf_2_forms ())
    {}
  };
}

dwarf_version const *
dwarf_2 ()
{
  static dwarf_2_t dw;
  return &dw;
}
