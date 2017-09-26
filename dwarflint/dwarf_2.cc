/* Pedantic checking of DWARF files
   Copyright (C) 2010, 2011 Red Hat, Inc.
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
#include "dwarf_2.hh"
#include "../libdw/dwarf.h"

namespace
{
  typedef preset_attribute<cl_reference, cl_constant> const_or_ref_attribute;

  struct dwarf_2_attributes
    : public attribute_table
  {
    dwarf_2_attributes ()
    {
      // Note about location descriptions in DWARF 2 (and 3).  In
      // DWARF 2, location expressions can have classes of cl_constant
      // or cl_block.  But we need to tell those block expressions
      // from any old block attribute to validate the expression, and
      // those constants from any old number to validate the
      // reference.  So we retrofit all the DW_FORM_block* forms and
      // appropriate attributes with cl_exprloc form DWARF 4 and
      // cl_loclistptr (even though in DWARF 4 it's actually only
      // DW_FORM_exprloc that has this class).
      //
      // Similarly with cl_lineptr and cl_macptr.  cl_rangelistptr
      // wasn't introduced until DWARF 3.

      add (ref_attribute (DW_AT_sibling));
      add (location_attribute (DW_AT_location));
      add (string_attribute (DW_AT_name));
      add (const_attribute (DW_AT_ordering));
      add (const_attribute (DW_AT_byte_size));
      add (const_attribute (DW_AT_bit_offset));
      add (const_attribute (DW_AT_bit_size));
      add (attribute (DW_AT_stmt_list, cl_lineptr));
      add (addr_attribute (DW_AT_low_pc));
      add (addr_attribute (DW_AT_high_pc));
      add (const_attribute (DW_AT_language));
      add (ref_attribute (DW_AT_discr));
      add (const_attribute (DW_AT_discr_value));
      add (const_attribute (DW_AT_visibility));
      add (ref_attribute (DW_AT_import));
      add (location_attribute (DW_AT_string_length));
      add (ref_attribute (DW_AT_common_reference));
      add (string_attribute (DW_AT_comp_dir));
      add (attribute (DW_AT_const_value,
		      dw_class_set (cl_string, cl_constant, cl_block)));
      add (ref_attribute (DW_AT_containing_type));
      add (ref_attribute (DW_AT_default_value));
      add (const_attribute (DW_AT_inline));
      add (flag_attribute (DW_AT_is_optional));
      add (const_or_ref_attribute (DW_AT_lower_bound));
      add (string_attribute (DW_AT_producer));
      add (flag_attribute (DW_AT_prototyped));
      add (location_attribute (DW_AT_return_addr));
      add (const_attribute (DW_AT_start_scope));
      add (const_attribute (DW_AT_bit_stride));
      add (const_or_ref_attribute (DW_AT_upper_bound));
      add (ref_attribute (DW_AT_abstract_origin));
      add (const_attribute (DW_AT_accessibility));
      add (const_attribute (DW_AT_address_class));
      add (flag_attribute (DW_AT_artificial));
      add (ref_attribute (DW_AT_base_types));
      add (const_attribute (DW_AT_calling_convention));
      add (const_or_ref_attribute (DW_AT_count));
      add (static_location_attribute (DW_AT_data_member_location));
      add (const_attribute (DW_AT_decl_column));
      add (const_attribute (DW_AT_decl_file));
      add (const_attribute (DW_AT_decl_line));
      add (flag_attribute (DW_AT_declaration));
      add (block_attribute (DW_AT_discr_list));
      add (const_attribute (DW_AT_encoding));
      add (flag_attribute (DW_AT_external));
      add (location_attribute (DW_AT_frame_base));
      add (ref_attribute (DW_AT_friend));
      add (const_attribute (DW_AT_identifier_case));
      add (attribute (DW_AT_macro_info, cl_macptr));
      add (block_attribute (DW_AT_namelist_item));
      add (ref_attribute (DW_AT_priority));
      add (location_attribute (DW_AT_segment));
      add (ref_attribute (DW_AT_specification));
      add (location_attribute (DW_AT_static_link));
      add (ref_attribute (DW_AT_type));
      add (location_attribute (DW_AT_use_location));
      add (flag_attribute (DW_AT_variable_parameter));
      add (const_attribute (DW_AT_virtuality));
      add (static_location_attribute (DW_AT_vtable_elem_location));
    }
  };

  struct exprloc_form
    : public preset_form<sc_block, cl_exprloc, cl_block>
  {
    exprloc_form (int a_name, form_width_t a_width)
      : preset_form<sc_block, cl_exprloc, cl_block> (a_name, a_width)
    {}
  };

  struct dwarf_2_forms
    : public form_table
  {
    dwarf_2_forms ()
    {
      add (exprloc_form (DW_FORM_block, fw_uleb));
      add (exprloc_form (DW_FORM_block1, fw_1));
      add (exprloc_form (DW_FORM_block2, fw_2));
      add (exprloc_form (DW_FORM_block4, fw_4));

      // These constant forms can in theory, in legal DWARF 2,
      // represent various pointers.
      typedef preset_form<sc_value,
			  cl_constant, cl_lineptr, cl_loclistptr,
			  cl_macptr> dw2_data_form;

      add (dw2_data_form (DW_FORM_data1, fw_1));
      add (dw2_data_form (DW_FORM_data2, fw_2));
      add (dw2_data_form (DW_FORM_data4, fw_4));
      add (dw2_data_form (DW_FORM_data8, fw_8));
      add (dw2_data_form (DW_FORM_sdata, fw_sleb));
      add (dw2_data_form (DW_FORM_udata, fw_uleb));

      add (flag_form (DW_FORM_flag, fw_1));

      add (ref_form (DW_FORM_ref1, fw_1));
      add (ref_form (DW_FORM_ref2, fw_2));
      add (ref_form (DW_FORM_ref4, fw_4));
      add (ref_form (DW_FORM_ref8, fw_8, fb_64));
      add (ref_form (DW_FORM_ref_udata, fw_uleb));

      add (string_form (DW_FORM_string));
      add (offset_form (DW_FORM_strp, cl_string));
      add (address_form (DW_FORM_addr, cl_address));
      add (address_form (DW_FORM_ref_addr, cl_reference));

      add (form (DW_FORM_indirect, cl_indirect, fw_uleb, sc_value));
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
