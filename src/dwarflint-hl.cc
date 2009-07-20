/* Pedantic checking of DWARF files.
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
#include "dwarflint-expected.hh"
#include "dwarfstrings.h"
#include "c++/dwarf"
#include "../libdw/libdwP.h"
#include "../libdw/c++/dwarf-knowledge.cc"

namespace
{
  message_category cat (message_category c1,
			message_category c2,
			message_category c3 = mc_none)
  {
    return static_cast<message_category> (c1 | c2 | c3);
  }
}

struct hl_ctx
{
  Dwarf *dwarf;
  elfutils::dwarf dw;

  hl_ctx (Elf *elf)
    : dwarf (dwarf_begin_elf (elf, DWARF_C_READ, NULL))
    , dw (dwarf)
  {
  }

  ~hl_ctx ()
  {
    dwarf_end (dwarf);
  }
};

hl_ctx *
hl_ctx_new (Elf *elf)
{
  return new hl_ctx (elf);
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
  struct where where_ref = WHERE (sec_info, NULL);
  struct where where_ar = WHERE (sec_aranges, NULL);
  where_ar.ref = &where_ref;
  struct where where_r = WHERE (sec_ranges, NULL);
  where_r.ref = &where_ref;
  char buf[128];

  const elfutils::dwarf::aranges_map &aranges = hlctx->dw.aranges ();
  for (elfutils::dwarf::aranges_map::const_iterator i = aranges.begin ();
       i != aranges.end (); ++i)
    {
      const elfutils::dwarf::compile_unit &cu = i->first;
      where_reset_1 (&where_ref, 0);
      where_reset_2 (&where_ref, cu.offset ());

      std::set<elfutils::dwarf::ranges::key_type>
	cu_aranges = i->second,
	cu_ranges = cu.ranges ();

      typedef std::vector <elfutils::dwarf::arange_list::value_type> range_vec;
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

struct name_extractor {
  int operator () (elfutils::dwarf::attribute const &at) {
    return at.first;
  }
} extract_name;

std::ostream &
operator << (std::ostream &o, elfutils::dwarf::value_space vs)
{
  using namespace elfutils;
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

static void
recursively_validate (elfutils::dwarf::compile_unit const &cu,
		      elfutils::dwarf::debug_info_entry const &parent)
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
  for (elfutils::dwarf::debug_info_entry::attributes_type::const_iterator jt
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
	  elfutils::dwarf::value_space vs = (*jt).second.what_space ();
	  if ((exp_vs & (1U << vs)) == 0)
	    wr_message (cat (mc_impact_3, mc_info), &where,
			": in DIE \"%s\", attribute \"%s\" has value of unexpected type \"%u\".\n",
			dwarf_tag_string (parent_tag),
			dwarf_attr_string (name),
			vs);
	}
      catch (...)
	{
	  wr_message (cat (mc_impact_4, mc_info, mc_error), &where,
		      ": in DIE \"%s\", couldn't obtain type of attribute \"%s\".\n",
		      dwarf_tag_string (parent_tag),
		      dwarf_attr_string (name));
	}
    }

  // Check children recursively.
  elfutils::dwarf::debug_info_entry::children_type const &children
    = parent.children ();
  for (elfutils::dwarf::debug_info_entry::children_type::const_iterator jt
	 = children.begin (); jt != children.end (); ++jt)
    recursively_validate (cu, *jt);
}

bool
check_expected_trees (hl_ctx *hlctx)
{
  class elfutils::dwarf::compile_units const &cus = hlctx->dw.compile_units ();
  for (elfutils::dwarf::compile_units::const_iterator it = cus.begin ();
       it != cus.end (); ++it)
    recursively_validate (*it, *it);

  return true;
}
