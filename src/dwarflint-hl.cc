/* Pedantic checking of DWARF files.  High level checks.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2009.

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

#include <iostream>
#include <set>
#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cassert>
#include <iterator>

#include "dwarflint.h"
#include "dwarflint-coverage.hh"
#include "dwarflint-expected.hh"
#include "dwarfstrings.h"
#include "c++/dwarf"
#include "../libdw/libdwP.h"
#include "../libdw/c++/dwarf-knowledge.cc"

using namespace elfutils;

class hl_ctx
{
  Dwarf *handle;

public:
  dwarf dw;

  hl_ctx (Elf *elf)
    : handle (dwarf_begin_elf (elf, DWARF_C_READ, NULL))
    , dw (handle)
  {
    // See if we can iterate compile units.  If not, this throws an
    // exception that gets caught in the C wrapper below.
    dw.compile_units ().begin ();
  }

  ~hl_ctx ()
  {
    dwarf_end (handle);
  }
};

hl_ctx *
hl_ctx_new (Elf *elf)
{
  try
    {
      return new hl_ctx (elf);
    }
  catch (std::runtime_error &exc)
    {
      wr_error (NULL, "Cannot initialize high-level checking: %s.\n",
		exc.what ());
      return NULL;
    }
}

void
hl_ctx_delete (hl_ctx *hlctx)
{
  delete hlctx;
}

static const expected_at_map expected_at;
//static const expected_children_map expected_children;

bool
check_matching_ranges (hl_ctx *hlctx)
{
  try
    {
      struct where where_ref = WHERE (sec_info, NULL);
      struct where where_ar = WHERE (sec_aranges, NULL);
      where_ar.ref = &where_ref;
      struct where where_r = WHERE (sec_ranges, NULL);
      where_r.ref = &where_ref;
      char buf[128];

      const dwarf::aranges_map &aranges = hlctx->dw.aranges ();
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

      return true;
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (NULL, "Error while checking matching ranges: %s.\n",
		exc.what ());
      return false;
    }
}

struct name_extractor {
  int operator () (dwarf::attribute const &at) {
    return at.first;
  }
} extract_name;

std::ostream &
operator << (std::ostream &o, dwarf::value_space vs)
{
  switch (vs)
    {
    case dwarf::VS_flag: return o << "flag";
    case dwarf::VS_dwarf_constant: return o << "dwarf_constant";
    case dwarf::VS_discr_list: return o << "discr_list";
    case dwarf::VS_reference: return o << "reference";
    case dwarf::VS_lineptr: return o << "lineptr";
    case dwarf::VS_macptr: return o << "macptr";
    case dwarf::VS_rangelistptr: return o << "rangelistptr";
    case dwarf::VS_identifier: return o << "identifier";
    case dwarf::VS_string: return o << "string";
    case dwarf::VS_source_file: return o << "source_file";
    case dwarf::VS_source_line: return o << "source_line";
    case dwarf::VS_source_column: return o << "source_column";
    case dwarf::VS_address: return o << "address";
    case dwarf::VS_constant: return o << "constant";
    case dwarf::VS_location: return o << "location";
    };

  abort ();
}

bool
check_expected_trees (hl_ctx *hlctx)
{
  try
    {
      struct
      {
	void operator () (dwarf::compile_unit const &cu,
			  dwarf::debug_info_entry const &parent)
	{
	  struct where where = WHERE (sec_info, NULL);
	  where_reset_1 (&where, cu.offset ());
	  where_reset_2 (&where, parent.offset ());

	  int parent_tag = parent.tag ();

	  // Set of attributes of this DIE.
	  std::set <int> attributes;
	  std::transform (parent.attributes ().begin (),
			  parent.attributes ().end (),
			  std::inserter (attributes, attributes.end ()),
			  extract_name);

	  // Attributes that we expect at this DIE.
	  expected_set::expectation_map const &expect
	    = expected_at.map (parent_tag);

	  // Check missing attributes.
	  for (expected_set::expectation_map::const_iterator jt
		 = expect.begin (); jt != expect.end (); ++jt)
	    {
	      std::set <int>::iterator kt = attributes.find (jt->first);
	      if (kt == attributes.end ())
		switch (jt->second)
		  {
		  case opt_required:
		    wr_message (cat (mc_impact_4, mc_info), &where,
				": %s lacks required attribute %s.\n",
				dwarf_tag_string (parent_tag),
				dwarf_attr_string (jt->first));
		    break;

		  case opt_expected:
		    wr_message (cat (mc_impact_2, mc_info), &where,
				": %s should contain attribute %s.\n",
				dwarf_tag_string (parent_tag),
				dwarf_attr_string (jt->first));
		  case opt_optional:
		    break;
		  };
	    }

	  // Check present attributes for expected-ness, and validate value space.
	  for (dwarf::debug_info_entry::attributes_type::const_iterator jt
		 = parent.attributes ().begin (), jte = parent.attributes ().end ();
	       jt != jte; ++jt)
	    {
	      unsigned name = extract_name (*jt);

	      expected_set::expectation_map::const_iterator kt = expect.find (name);
	      if (kt == expect.end ())
		wr_message (cat (mc_impact_3, mc_info), &where,
			    ": DIE \"%s\" has attribute \"%s\", which is not expected.\n",
			    dwarf_tag_string (parent_tag),
			    dwarf_attr_string (name));
	      try
		{
		  unsigned exp_vs = expected_value_space (name, parent_tag);
		  dwarf::value_space vs = (*jt).second.what_space ();
		  if ((exp_vs & (1U << vs)) == 0)
		    wr_message (cat (mc_impact_3, mc_info), &where,
				": in DIE \"%s\", attribute \"%s\" has value of unexpected type \"%u\".\n",
				dwarf_tag_string (parent_tag),
				dwarf_attr_string (name),
				vs);
		}
	      // XXX more specific class when <dwarf> has it
	      catch (...)
		{
		  wr_message (cat (mc_impact_4, mc_info, mc_error), &where,
			      ": in DIE \"%s\", couldn't obtain type of attribute \"%s\".\n",
			      dwarf_tag_string (parent_tag),
			      dwarf_attr_string (name));
		}
	    }

	  // Check children recursively.
	  dwarf::debug_info_entry::children_type const &children
	    = parent.children ();
	  for (dwarf::debug_info_entry::children_type::const_iterator
		 jt = children.begin (); jt != children.end (); ++jt)
	    (*this) (cu, *jt);
	}
      } recursively_validate;

      class dwarf::compile_units const &cus = hlctx->dw.compile_units ();
      for (dwarf::compile_units::const_iterator it = cus.begin ();
	   it != cus.end (); ++it)
	recursively_validate (*it, *it);
      return true;
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (NULL, "Error while checking expected trees: %s.\n",
		exc.what ());
      return false;
    }
}

bool
check_range_out_of_scope (hl_ctx *hlctx)
{
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

      class dwarf::compile_units const &cus = hlctx->dw.compile_units ();
      ranges_t r;
      r.push_back (std::make_pair (0, -1));
      where wh = WHERE (sec_info, NULL);
      for (dwarf::compile_units::const_iterator it = cus.begin ();
	   it != cus.end (); ++it)
	recursively_validate (*it, *it, r, wh);
      return true;
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (NULL, "Error while checking range out of scope: %s.\n",
		exc.what ());
      return false;
    }
}
