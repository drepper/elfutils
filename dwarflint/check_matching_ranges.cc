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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "highlevel_check.hh"
#include "check_debug_loc_range.hh"
#include "check_debug_aranges.hh"

using elfutils::dwarf;

namespace
{
  class check_matching_ranges
    : public highlevel_check<check_matching_ranges>
  {
  public:
    static checkdescriptor descriptor () {
      static checkdescriptor cd ("check_matching_ranges");
      return cd;
    }

    check_matching_ranges (checkstack &stack, dwarflint &lint);
  };

  reg<check_matching_ranges> reg_matching_ranges;
}

check_matching_ranges::check_matching_ranges (checkstack &stack,
					      dwarflint &lint)
  : highlevel_check<check_matching_ranges> (stack, lint)
{
  if (be_tolerant || be_gnu)
    throw check_base::unscheduled ();

  lint.check<check_debug_ranges> (stack);
  lint.check<check_debug_aranges> (stack);

  try
    {
      struct where where_ref = WHERE (sec_info, NULL);
      struct where where_ar = WHERE (sec_aranges, NULL);
      where_ar.ref = &where_ref;
      struct where where_r = WHERE (sec_ranges, NULL);
      where_r.ref = &where_ref;
      char buf[128];

      const dwarf::aranges_map &aranges = dw.aranges ();
      for (dwarf::aranges_map::const_iterator i = aranges.begin ();
	   i != aranges.end (); ++i)
	{
	  const dwarf::compile_unit &cu = i->first;
	  where_reset_1 (&where_ref, 0);
	  where_reset_2 (&where_ref, cu.offset ());

	  std::set<dwarf::ranges::key_type>
	    cu_aranges = i->second,
	    cu_ranges = cu.ranges ();

	  typedef std::vector <dwarf::arange_list::value_type>
	    range_vec;
	  range_vec missing;
	  std::back_insert_iterator <range_vec> i_missing (missing);

	  std::set_difference (cu_aranges.begin (), cu_aranges.end (),
			       cu_ranges.begin (), cu_ranges.end (),
			       i_missing);

	  for (range_vec::iterator it = missing.begin ();
	       it != missing.end (); ++it)
	    wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_r,
			": missing range %s, present in .debug_aranges.\n",
			range_fmt (buf, sizeof buf, it->first, it->second));

	  missing.clear ();
	  std::set_difference (cu_ranges.begin (), cu_ranges.end (),
			       cu_aranges.begin (), cu_aranges.end (),
			       i_missing);

	  for (range_vec::iterator it = missing.begin ();
	       it != missing.end (); ++it)
	    wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_ar,
			": missing range %s, present in .debug_ranges.\n",
			range_fmt (buf, sizeof buf, it->first, it->second));
	}
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (WHERE (sec_info, NULL))
	<< "Exception while checking matching ranges: " << exc.what ()
	<< std::endl;
      throw check_base::failed ();
    }
}
