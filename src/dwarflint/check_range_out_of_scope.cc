/* Check whether PC ranges reported at DIE fall into the containing scope.
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
#include "coverage.hh"
#include "pri.hh"
#include "check_debug_loc_range.hh"

using elfutils::dwarf;

namespace
{
  class check_range_out_of_scope
    : public highlevel_check<check_range_out_of_scope>
  {
    typedef std::vector<std::pair< ::Dwarf_Addr, ::Dwarf_Addr> >
      ranges_t;

    static void recursively_validate (dwarf::compile_unit const &cu,
				      dwarf::debug_info_entry const &die,
				      ranges_t const &ranges,
				      where const &wh_parent);

  public:
    explicit check_range_out_of_scope (dwarflint &lint);
  };

  // Register the check.
  reg<check_range_out_of_scope> reg_range_out_of_scope;
}

check_range_out_of_scope::check_range_out_of_scope (dwarflint &lint)
  : highlevel_check<check_range_out_of_scope> (lint)
{
  lint.check <check_debug_loc> ();

  try
    {
      class dwarf::compile_units const &cus = dw.compile_units ();
      ranges_t r;
      r.push_back (std::make_pair (0, -1));
      where wh = WHERE (sec_info, NULL);
      for (dwarf::compile_units::const_iterator it = cus.begin ();
	   it != cus.end (); ++it)
	recursively_validate (*it, *it, r, wh);
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (WHERE (sec_info, NULL))
	<< "Exception while checking ranges out of scope: " << exc.what ()
	<< std::endl;
      throw check_base::failed ();
    }
}

void
check_range_out_of_scope::recursively_validate
  (dwarf::compile_unit const &cu,
   dwarf::debug_info_entry const &die,
   ranges_t const &ranges,
   where const &wh_parent)
{
  where wh = WHERE (sec_info, NULL);
  where_reset_1 (&wh, cu.offset ());
  where_reset_2 (&wh, die.offset ());

  ::Dwarf_Addr low_pc = 0;
  ::Dwarf_Addr high_pc = -1;
  ranges_t my_ranges;
  for (dwarf::debug_info_entry::attributes_type::const_iterator
	 at = die.attributes ().begin ();
       at != die.attributes ().end (); ++at)
    {
      dwarf::attr_value const &value = (*at).second;
      dwarf::value_space vs = value.what_space ();
      if ((*at).first == DW_AT_low_pc)
	low_pc = value.address ();
      else if ((*at).first == DW_AT_high_pc)
	high_pc = value.address ();
      else if (vs == dwarf::VS_rangelistptr)
	for (dwarf::range_list::const_iterator
	       it = value.ranges ().begin ();
	     it != value.ranges ().end (); ++it)
	  my_ranges.push_back (*it);
    }

  if (low_pc != 0 || high_pc != (::Dwarf_Addr)-1)
    {
      // Simultaneous appearance of both low_pc/high_pc pair
      // and rangelist pointer is forbidden by 3.1.1 #1.
      // Presence of low_pc on itself is OK on compile_unit
      // and partial_unit DIEs, otherwise it serves the same
      // purpose as low_pc/high_pc pair that covers one
      // address point.

      if (high_pc == (::Dwarf_Addr)-1
	  && die.tag () != DW_TAG_compile_unit
	  && die.tag () != DW_TAG_partial_unit)
	high_pc = low_pc + 1;

      if (high_pc != (::Dwarf_Addr)-1)
	{
	  if (my_ranges.size () != 0)
	    wr_message (wh, cat (mc_impact_4, mc_info, mc_error))
	      << "both low_pc/high_pc pair and ranges present."
	      << std::endl;
	  else
	    my_ranges.push_back (std::make_pair (low_pc, high_pc));
	}
    }

  // If my_ranges is non-empty, check that it's a subset of
  // ranges.
  if (my_ranges.size () != 0)
    {
      // xxx Extract this logic to some table.
      switch (die.tag ())
	{
	  /* These PC-ful DIEs should be wholly contained by
	     PC-ful parental DIE.  */
	case DW_TAG_inlined_subroutine:
	case DW_TAG_lexical_block:
	case DW_TAG_entry_point:
	case DW_TAG_label:
	case DW_TAG_with_stmt:
	case DW_TAG_try_block:
	case DW_TAG_catch_block:
	  {
	    coverage cov1;
	    WIPE (cov1);

	    for (ranges_t::const_iterator it = my_ranges.begin ();
		 it != my_ranges.end (); ++it)
	      coverage_add (&cov1, (*it).first, (*it).second - (*it).first);

	    coverage cov2;
	    WIPE (cov2);
	    for (ranges_t::const_iterator it = ranges.begin ();
		 it != ranges.end (); ++it)
	      coverage_add (&cov2, (*it).first, (*it).second - (*it).first);

	    coverage result;
	    WIPE (result);
	    coverage_add_all (&result, &cov1);
	    coverage_remove_all (&result, &cov2);

	    if (result.size > 0)
	      {
		wr_error (wh)
		  << "PC range " << cov::format_ranges (cov1)
		  << " is not a sub-range of containing scope."
		  << std::endl;

		wr_error (wh_parent)
		  << "in this context: " << cov::format_ranges (cov2)
		  << std::endl;
	      }

	    coverage_free (&result);
	    coverage_free (&cov2);
	    coverage_free (&cov1);
	  }
	}
    }

  // xxx building the coverage for each die is a waste of time
  ranges_t const &use_ranges
    = my_ranges.size () > 0 ? my_ranges : ranges;
  coverage cov;
  WIPE (cov);

  for (ranges_t::const_iterator it = use_ranges.begin ();
       it != use_ranges.end (); ++it)
    coverage_add (&cov, (*it).first, (*it).second - (*it).first);

  // Now finally look for location attributes and check that
  // _their_ PCs form a subset of ranges of this DIE.
  for (dwarf::debug_info_entry::attributes_type::const_iterator
	 at = die.attributes ().begin ();
       at != die.attributes ().end (); ++at)
    {
      dwarf::attr_value const &value = (*at).second;
      dwarf::value_space vs = value.what_space ();

      if (vs == dwarf::VS_location)
	{
	  dwarf::location_attr const &loc = value.location ();
	  if (loc.is_list ())
	    {
	      bool runoff = false;
	      for (dwarf::location_attr::const_iterator
		     lt = loc.begin (); lt != loc.end (); ++lt)
		{
		  ::Dwarf_Addr start = (*lt).first.first; //1st insn
		  ::Dwarf_Addr end = (*lt).first.second; //1st past end
		  ::Dwarf_Addr length = end - start;
		  if (length > 0 // skip empty ranges
		      && !coverage_is_covered (&cov, start, length))
		    {
		      runoff = true;
		      wr_error (wh)
			<< "attribute `" << pri::attr ((*at).first)
			<< "': PC range " << pri::range (start, end)
			<< " outside containing scope." << std::endl;
		    }
		}
	      if (runoff)
		wr_error (wh_parent)
		  << "in this context: " << cov::format_ranges (cov)
		  << '.' << std::endl;
	    }
	}
    }

  coverage_free (&cov);

  // Check children recursively.
  for (dwarf::debug_info_entry::children_type::const_iterator
	 jt = die.children ().begin ();
       jt != die.children ().end (); ++jt)
    recursively_validate (cu, *jt, use_ranges,
			  my_ranges.size () > 0 ? wh : wh_parent);
}
