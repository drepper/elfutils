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
#include "../libdw/dwarf.h"

namespace
{
  struct dwarf_gnu_attributes
    : public attribute_table
  {
    void unused (__attribute__ ((unused)) attribute const &attrib) const {}
    dwarf_gnu_attributes ()
    {
      // It's rather hard to find any reference on some of the GNU
      // extensions.  So we simply mark these with unused.

      // The MIPS documentation claims that these are "apparently only
      // output in DWARF1, not DWARF2".  I found nothing particular
      // about how these are used.
      unused (const_attribute (DW_AT_sf_names));
      unused (const_attribute (DW_AT_src_info));
      unused (const_attribute (DW_AT_mac_info));
      unused (const_attribute (DW_AT_src_coords));
      unused (const_attribute (DW_AT_body_begin));
      unused (const_attribute (DW_AT_body_end));

      add (flag_attribute (DW_AT_GNU_vector));

      // http://gcc.gnu.org/wiki/ThreadSafetyAnnotationsInDWARF

      // xxx these are glass cl_GNU_mutexlistptr.  data4 and data8 are
      // supposed to have this class.  So how do we smuggle this class
      // to whatever DW_FORM_data4 and DW_FORM_data8 have in current
      // version?  For now, just claim it's plain old constant.
      add (const_attribute (DW_AT_GNU_guarded_by));
      add (const_attribute (DW_AT_GNU_pt_guarded_by));
      add (const_attribute (DW_AT_GNU_guarded));
      add (const_attribute (DW_AT_GNU_pt_guarded));
      add (const_attribute (DW_AT_GNU_locks_excluded));
      add (const_attribute (DW_AT_GNU_exclusive_locks_required));
      add (const_attribute (DW_AT_GNU_shared_locks_required));

      // Contains a shallower 8-byte signature of the type described
      // in the type unit.  We encode it the same way as
      // DW_AT_signature, which AFAICT is just a standardized name of
      // DW_AT_GNU_odr_signature.
      // http://gcc.gnu.org/wiki/DwarfSeparateTypeInfo
      // http://wiki.dwarfstd.org/index.php?title=COMDAT_Type_Sections
      add (ref_attribute (DW_AT_GNU_odr_signature));

      add (string_attribute (DW_AT_GNU_template_name)); // xxx ???
    }
  };

  struct dwarf_gnu_ext_t
    : public std_dwarf
  {
    dwarf_gnu_ext_t ()
      : std_dwarf (dwarf_gnu_attributes (), form_table ())
    {}
  };
}

dwarf_version const *
dwarf_gnu_ext ()
{
  static dwarf_gnu_ext_t dw;
  return &dw;
}
