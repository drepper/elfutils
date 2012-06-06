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
#include "../libdw/dwarf.h"

namespace
{
  struct dwarf_mips_attributes
    : public attribute_table
  {
    void unused (__attribute__ ((unused)) attribute const &attrib) const {}
    dwarf_mips_attributes ()
    {
      // Most of these are really just sketched, since we don't emit
      // them anyway.  For those in need, the documentation is in
      // mips_extensions.pdf that's installed by libdwarf-devel in
      // Fedora.  According to that document, some forms were never
      // even emitted.  Those are marked as unused and not added.
      // Their class is arbitrarily chosen as cl_constant.

      add (const_attribute (DW_AT_MIPS_fde));
      unused (const_attribute (DW_AT_MIPS_loop_begin));
      unused (const_attribute (DW_AT_MIPS_tail_loop_begin));
      unused (const_attribute (DW_AT_MIPS_epilog_begin));
      unused (const_attribute (DW_AT_MIPS_loop_unroll_factor));
      unused (const_attribute (DW_AT_MIPS_software_pipeline_depth));
      add (string_attribute (DW_AT_MIPS_linkage_name));

      // [section 8.10] If DW_AT_MIPS_stride is present, the attribute
      // contains a reference to a DIE which describes the location
      // holding the stride, and the DW_AT_stride_size field of
      // DW_TAG_array_type is ignored if present. The value of the
      // stride is the number of 4 byte words between elements along
      // that axis.
      add (ref_attribute (DW_AT_MIPS_stride));

      add (string_attribute (DW_AT_MIPS_abstract_name));

      // xxx in addition, this is supposed to be CU-local reference,
      // similarly to the DW_AT_sibling.  An opportunity to generalize
      // sibling_form_suitable.
      add (ref_attribute (DW_AT_MIPS_clone_origin));

      add (flag_attribute (DW_AT_MIPS_has_inlines));

      // The documentation is unclear on what form these should take.
      // I'm making them the same as DW_AT_byte_stride in DWARF2, in
      // hopes that that's what they are supposed to be.
      add (const_attribute (DW_AT_MIPS_stride_byte));
      add (const_attribute (DW_AT_MIPS_stride_elem));

      add (ref_attribute (DW_AT_MIPS_ptr_dopetype));
      add (ref_attribute (DW_AT_MIPS_allocatable_dopetype));
      add (ref_attribute (DW_AT_MIPS_assumed_shape_dopetype));
      add (flag_attribute (DW_AT_MIPS_assumed_size));
    }
  };

  struct dwarf_mips_ext_t
    : public std_dwarf
  {
    dwarf_mips_ext_t ()
      : std_dwarf (dwarf_mips_attributes (), form_table ())
    {}
  };
}

dwarf_version const *
dwarf_mips_ext ()
{
  static dwarf_mips_ext_t dw;
  return &dw;
}
