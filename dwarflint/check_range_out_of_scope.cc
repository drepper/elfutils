/* Check whether PC ranges reported at DIE fall into the containing scope.
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
				      locus const &wh_parent);

  public:
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("check_range_out_of_scope")
	 .description (
"Check whether PC ranges reported at DIEs fall into the containing scope.\n"));
      return &cd;
    }

    check_range_out_of_scope (checkstack &stack, dwarflint &lint);
  };

  // Register the check.
  reg<check_range_out_of_scope> reg_range_out_of_scope;
  const ::Dwarf_Addr noaddr = -1;
}

check_range_out_of_scope::check_range_out_of_scope (checkstack &stack, dwarflint &lint)
  : highlevel_check<check_range_out_of_scope> (stack, lint)
{
  try
    {
      class dwarf::compile_units_type const &cus = dw.compile_units ();
      ranges_t r;
      r.push_back (std::make_pair (0, -1));
      section_locus wh (sec_info);
      for (dwarf::compile_units_type::const_iterator it = cus.begin ();
	   it != cus.end (); ++it)
	recursively_validate (*it, *it, r, wh);
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (section_locus (sec_info))
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
   locus const &wh_parent)
{
  die_locus wh (die);

  ::Dwarf_Addr low_pc = 0;
  ::Dwarf_Addr high_pc = ::noaddr;
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

  if (low_pc != 0 || high_pc != ::noaddr)
    {
      // Simultaneous appearance of both low_pc/high_pc pair
      // and rangelist pointer is forbidden by 3.1.1 #1.
      // Presence of low_pc on itself is OK on compile_unit
      // and partial_unit DIEs, otherwise it serves the same
      // purpose as low_pc/high_pc pair that covers one
      // address point.

      if (high_pc == ::noaddr
	  && die.tag () != DW_TAG_compile_unit
	  && die.tag () != DW_TAG_partial_unit)
	high_pc = low_pc + 1;

      if (high_pc != ::noaddr)
	{
	  if (my_ranges.size () != 0)
	    wr_message (wh, mc_impact_4 | mc_info | mc_error)
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
	    for (ranges_t::const_iterator it = my_ranges.begin ();
		 it != my_ranges.end (); ++it)
	      cov1.add ((*it).first, (*it).second - (*it).first);

	    coverage cov2;
	    for (ranges_t::const_iterator it = ranges.begin ();
		 it != ranges.end (); ++it)
	      cov2.add ((*it).first, (*it).second - (*it).first);

	    coverage result = cov1 - cov2;

	    if (!result.empty ())
	      {
		wr_message (wh, mc_error).id (descriptor ())
		  << "PC range " << cov::format_ranges (cov1)
		  << " is not a sub-range of containing scope."
		  << std::endl;

		wr_message (wh_parent, mc_error).when_prev ()
		  << "in this context: " << cov::format_ranges (cov2)
		  << std::endl;
	      }
	  }
	}
    }

  // xxx building the coverage for each die is a waste of time
  ranges_t const &use_ranges
    = my_ranges.size () > 0 ? my_ranges : ranges;
  coverage cov;

  for (ranges_t::const_iterator it = use_ranges.begin ();
       it != use_ranges.end (); ++it)
    cov.add ((*it).first, (*it).second - (*it).first);

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
		      && !cov.is_covered (start, length))
		    wr_message (wh, mc_error)
		      .id (descriptor (), runoff)
		      << "attribute `"
		      << elfutils::dwarf::attributes::name ((*at).first)
		      << "': PC range " << pri::range (start, end)
		      << " outside containing scope." << std::endl;
		}
	      wr_message (wh_parent, mc_error)
		.when (runoff)
		<< "in this context: " << cov::format_ranges (cov)
		<< '.' << std::endl;
	    }
	}
    }

  // Check children recursively.
  for (dwarf::debug_info_entry::children_type::const_iterator
	 jt = die.children ().begin ();
       jt != die.children ().end (); ++jt)
    recursively_validate (cu, *jt, use_ranges,
			  my_ranges.size () > 0 ? wh : wh_parent);
}
