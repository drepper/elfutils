/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("check_matching_ranges")
	 .description (
"Check that the ranges in .debug_aranges and .debug_ranges match.\n"));
      return &cd;
    }

    check_matching_ranges (checkstack &stack, dwarflint &lint);
  };

  reg<check_matching_ranges> reg_matching_ranges;
}

check_matching_ranges::check_matching_ranges (checkstack &stack,
					      dwarflint &lint)
  : highlevel_check<check_matching_ranges> (stack, lint)
{
  lint.check<check_debug_ranges> (stack);
  lint.check<check_debug_aranges> (stack);

  try
    {
      char buf[128];
      const dwarf::aranges_map &aranges = dw.aranges ();
      for (dwarf::aranges_map::const_iterator i = aranges.begin ();
	   i != aranges.end (); ++i)
	{
	  const dwarf::compile_unit &cu = i->first;
	  cudie_locus where_ref (cu);
	  loc_range_locus where_r (sec_ranges, where_ref);
	  arange_locus where_ar (where_ref);

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
	    wr_message (where_r, mc_ranges | mc_aranges | mc_impact_3)
	      << "missing range "
	      << range_fmt (buf, sizeof buf, it->first, it->second)
	      << ", present in .debug_aranges." << std::endl;

	  missing.clear ();
	  std::set_difference (cu_ranges.begin (), cu_ranges.end (),
			       cu_aranges.begin (), cu_aranges.end (),
			       i_missing);

	  for (range_vec::iterator it = missing.begin ();
	       it != missing.end (); ++it)
	    wr_message (where_ar, mc_ranges | mc_aranges | mc_impact_3)
	      << "missing range "
	      << range_fmt (buf, sizeof buf, it->first, it->second)
	      << ", present in .debug_ranges." << std::endl;
	}
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (section_locus (sec_info))
	<< "Exception while checking matching ranges: " << exc.what ()
	<< std::endl;
      throw check_base::failed ();
    }
}
