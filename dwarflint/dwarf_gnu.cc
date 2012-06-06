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

      // xxx these are glass cl_GNU_mutexlistptr.  data4 and data8 are
      // supposed to have this class.  So how do we smuggle this class
      // to whatever DW_FORM_data4 and DW_FORM_data8 have in current
      // version?  For now, just claim it's plain old constant.
      // http://gcc.gnu.org/wiki/ThreadSafetyAnnotationsInDWARF
      add (const_attribute (DW_AT_GNU_guarded_by));
      add (const_attribute (DW_AT_GNU_pt_guarded_by));
      add (const_attribute (DW_AT_GNU_guarded));
      add (const_attribute (DW_AT_GNU_pt_guarded));
      add (const_attribute (DW_AT_GNU_locks_excluded));
      add (const_attribute (DW_AT_GNU_exclusive_locks_required));
      add (const_attribute (DW_AT_GNU_shared_locks_required));

      // Contains a shallower 8-byte signature of the type described
      // in the type unit.  This is nominally a const_attribute, but
      // we do the checking ourselves in form_allowed.
      // http://gcc.gnu.org/wiki/DwarfSeparateTypeInfo
      add (const_attribute (DW_AT_GNU_odr_signature));

      // http://gcc.gnu.org/wiki/TemplateParmsDwarf
      add (string_attribute (DW_AT_GNU_template_name));

      // GNU extensions for representation of call sites
      // http://www.dwarfstd.org/ShowIssue.php?issue=100909.2
      add (attribute (DW_AT_GNU_call_site_value, cl_exprloc));
      add (attribute (DW_AT_GNU_call_site_data_value, cl_exprloc));
      add (attribute (DW_AT_GNU_call_site_target, cl_exprloc));
      add (attribute (DW_AT_GNU_call_site_target_clobbered, cl_exprloc));
      add (flag_attribute (DW_AT_GNU_tail_call));
      add (flag_attribute (DW_AT_GNU_all_tail_call_sites));
      add (flag_attribute (DW_AT_GNU_all_call_sites));
      add (flag_attribute (DW_AT_GNU_all_source_call_sites));
    }
  };

  struct dwarf_gnu_ext_t
    : public std_dwarf
  {
    dwarf_gnu_ext_t ()
      : std_dwarf (dwarf_gnu_attributes (), form_table ())
    {}

    virtual bool
    form_allowed (attribute const *attr, form const *form) const
    {
      // Without -gstrict-dwarf gcc allows usage of attributes from
      // later versions. One strange case is DW_AT_ranges in version 2
      // since that version doesn't actually define a rangelistptr
      // class. So we just allow data4 or data8 here.
      if (attr->name () == DW_AT_ranges)
	{
	  form_width_t width = form->width (NULL);
	  return (form->classes ()[cl_constant]
		  && (width == fw_4 || width == fw_8));
	}

      // upper_bound is allowed to also be a block (dwarf3 in dwarf2).
      if (attr->name () == DW_AT_upper_bound)
	return form->classes ()[cl_block];

      if (attr->name () == DW_AT_GNU_odr_signature)
	return form->classes ()[cl_constant] && form->width (NULL) == fw_8;
      else
	return std_dwarf::form_allowed (attr, form);
    }
  };
}

dwarf_version const *
dwarf_gnu_ext ()
{
  static dwarf_gnu_ext_t dw;
  return &dw;
}
