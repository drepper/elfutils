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
#include "dwarf_3.hh"
#include "dwarf_4.hh"
#include "../libdw/dwarf.h"

namespace
{
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

  struct dwarf_4_ext_t
    : public std_dwarf
  {
    dwarf_4_ext_t ()
      : std_dwarf (dwarf_4_attributes (), dwarf_4_forms ())
    {}
  };
}

dwarf_version const *
dwarf_4_ext ()
{
  static dwarf_4_ext_t dw;
  return &dw;
}

dwarf_version const *
dwarf_4 ()
{
  static dwarf_version const *dw =
    dwarf_version::extend (dwarf_3 (), dwarf_4_ext ());
  return dw;
}
