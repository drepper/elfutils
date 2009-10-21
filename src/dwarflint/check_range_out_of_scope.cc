#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "checks-high.hh"
#include "coverage.hh"
#include "dwarfstrings.h"

using elfutils::dwarf;

namespace
{
  class check_range_out_of_scope
    : public highlevel_check<check_range_out_of_scope>
  {
  public:
    explicit check_range_out_of_scope (dwarflint &lint);
  };

  reg<check_range_out_of_scope> reg_matching_ranges;
}

check_range_out_of_scope::check_range_out_of_scope (dwarflint &lint)
  : highlevel_check<check_range_out_of_scope> (lint)
{
  lint.check <check_debug_loc> ();

  try
    {
      typedef std::vector<std::pair< ::Dwarf_Addr, ::Dwarf_Addr> >
	ranges_t;

      struct
      {
	void operator () (dwarf::compile_unit const &cu,
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
		    wr_message (cat (mc_impact_4, mc_info, mc_error), &wh,
				": both low_pc/high_pc pair and ranges present.\n");
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
		      coverage_add (&cov1, (*it).first,
				    (*it).second - (*it).first);

		    coverage cov2;
		    WIPE (cov2);
		    for (ranges_t::const_iterator it = ranges.begin ();
			 it != ranges.end (); ++it)
		      coverage_add (&cov2, (*it).first,
				    (*it).second - (*it).first);

		    coverage result;
		    WIPE (result);
		    coverage_add_all (&result, &cov1);
		    coverage_remove_all (&result, &cov2);

		    if (result.size > 0)
		      {
			std::string super_wh = where_fmt (&wh_parent);
			{
			  std::string rs = cov::format_ranges (cov1);
			  wr_error (&wh, ": PC range %s is not a sub-range of "
				    "containing scope.\n", rs.c_str ());
			}
			{
			  std::string rs = cov::format_ranges (cov2);
			  wr_error (&wh_parent, ": in this context: %s\n",
				    rs.c_str ());
			}
		      }

		    coverage_free (&result);
		    coverage_free (&cov2);
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
			      std::string super_wh = where_fmt (&wh_parent);
			      wr_error (&wh, ": attribute `%s': PC range %s "
					"outside containing scope\n",
					dwarf_attr_string ((*at).first),
					range_fmt (start, end).c_str ());
			    }
			}
		      if (runoff)
			{
			  std::string rangestr = cov::format_ranges (cov);
			  wr_error (&wh_parent, ": in this context: %s\n",
				    rangestr.c_str ());
			}
		    }
		}
	    }

	  coverage_free (&cov);

	  // Check children recursively.
	  for (dwarf::debug_info_entry::children_type::const_iterator
		 jt = die.children ().begin ();
	       jt != die.children ().end (); ++jt)
	    (*this) (cu, *jt, use_ranges,
		     my_ranges.size () > 0 ? wh : wh_parent);
	}
      } recursively_validate;

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
      throw check_base::failed
	(std::string ("Error while checking range out of scope: ")
	 + exc.what () + ".\n");
    }
}
