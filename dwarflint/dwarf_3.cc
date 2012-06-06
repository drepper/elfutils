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
#include "dwarf_2.hh"
#include "dwarf_3.hh"
#include "../libdw/dwarf.h"
#include <cassert>

namespace
{
  struct dwarf_3_attributes
    : public attribute_table
  {
    dwarf_3_attributes ()
    {
      add (location_attribute (DW_AT_location));
      add (dynval_attribute (DW_AT_byte_size));
      add (dynval_attribute (DW_AT_bit_offset));
      add (dynval_attribute (DW_AT_bit_size));
      add (location_attribute (DW_AT_string_length));
      add (dynval_attribute (DW_AT_lower_bound));
      add (location_attribute (DW_AT_return_addr));

      // Note, DWARF 3 claims only a const class for DW_AT_bit_stride,
      // but from 2.19 it's clear that this is an omission.
      add (dynval_attribute (DW_AT_bit_stride));

      add (dynval_attribute (DW_AT_upper_bound));
      add (dynval_attribute (DW_AT_count));
      add (attribute (DW_AT_data_member_location,
		      dw_class_set (cl_exprloc, cl_constant, cl_loclistptr)));
      add (location_attribute (DW_AT_frame_base));
      add (location_attribute (DW_AT_segment));
      add (location_attribute (DW_AT_static_link));
      add (location_attribute (DW_AT_use_location));
      add (location_attribute (DW_AT_vtable_elem_location));
      add (dynval_attribute (DW_AT_allocated));
      add (dynval_attribute (DW_AT_associated));
      add (attribute (DW_AT_data_location, cl_exprloc));
      add (dynval_attribute (DW_AT_byte_stride));
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

  struct dwarf_3_forms
    : public form_table
  {
    dwarf_3_forms ()
    {
      add (offset_form (DW_FORM_ref_addr, cl_reference));

      // In DWARF 2 we made all the const forms into various cl_*ptr,
      // since that's how the standard was worded: it allowed
      // DW_AT_location to have any constant form.  In DWARF 3, only
      // data4 and data8 are like this.  In addition, these two can
      // also be cl_rangelistptr.
      typedef preset_form<sc_value,
			  cl_constant, cl_lineptr, cl_loclistptr,
			  cl_macptr, cl_rangelistptr> dw3_data_form;

      add (const_form (DW_FORM_data1, fw_1));
      add (const_form (DW_FORM_data2, fw_2));
      add (dw3_data_form (DW_FORM_data4, fw_4));
      add (dw3_data_form (DW_FORM_data8, fw_8));
      add (const_form (DW_FORM_sdata, fw_sleb));
      add (const_form (DW_FORM_udata, fw_uleb));
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
