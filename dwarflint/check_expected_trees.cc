/*
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

#include "../libdw/c++/dwarf-knowledge.cc"
#include "../libdw/c++/dwarf"

#include "check_debug_info.hh"
#include "highlevel_check.hh"
#include "expected.hh"
#include "messages.hh"

using elfutils::dwarf;

namespace
{
  class check_expected_trees
    : public highlevel_check<check_expected_trees>
  {
  public:
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("check_expected_trees")
	 .description (
"Checks whether all DIEs have the right attributes and the right children.\n"
"Currently this is very much a work in progress.\n"));
      return &cd;
    }

    check_expected_trees (checkstack &stack, dwarflint &lint);
  };

  reg<check_expected_trees> reg_check_expected_trees;

  const expected_at_map expected_at;
  //static const expected_children_map expected_children;

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
}

check_expected_trees::check_expected_trees (checkstack &stack, dwarflint &lint)
  : highlevel_check<check_expected_trees> (stack, lint)
{
  lint.check <check_debug_info> (stack);

  try
    {
      struct
      {
	void operator () (dwarf::compile_unit const &cu,
			  dwarf::debug_info_entry const &parent)
	{
	  die_locus where (parent);

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
	      char const *what = NULL;
	      if (kt == attributes.end ())
		switch (jt->second)
		  {
		  case opt_required:
		    what = " lacks required attribute ";
		    // FALL_THROUGH

		  case opt_expected:
		    if (what == NULL)
		      what = " should contain attribute ";
		    wr_message (where, mc_impact_2 | mc_info)
		      << elfutils::dwarf::tags::name (parent_tag) << what
		      << elfutils::dwarf::attributes::name (jt->first) << '.'
		      << std::endl;
		    break;

		  case opt_optional:
		    break;
		  };
	    }

	  // Check present attributes for expected-ness, and validate
	  // value space.
	  for (dwarf::debug_info_entry::attributes_type::const_iterator
		 jt = parent.attributes ().begin (),
		 jte = parent.attributes ().end ();
	       jt != jte; ++jt)
	    {
	      unsigned name = extract_name (*jt);

	      expected_set::expectation_map::const_iterator
		kt = expect.find (name);
	      if (kt == expect.end ())
		wr_message (where, mc_impact_3 | mc_info)
		  << ": DIE \"" << dwarf::tags::name (parent_tag)
		  << "\" has attribute \"" << dwarf::attributes::name (name)
		  << "\", which is not expected." << std::endl;

	      try
		{
		  unsigned exp_vs = expected_value_space (name, parent_tag);
		  dwarf::value_space vs = (*jt).second.what_space ();
		  if ((exp_vs & (1U << vs)) == 0)
		    wr_message (where, mc_impact_3 | mc_info)
		      << ": in DIE \"" << dwarf::tags::name (parent_tag)
		      << "\", attribute \"" << dwarf::attributes::name (name)
		      << "\" has value of unexpected type \"" << vs
		      << "\"." << std::endl;
		}
	      // XXX more specific class when <dwarf> has it
	      catch (...)
		{
		  wr_message (where, mc_impact_4 | mc_info | mc_error)
		    << ": in DIE \"" << dwarf::tags::name (parent_tag)
		    << "\", couldn't obtain type of attribute \""
		    << dwarf::attributes::name (name) << "\"."
		    << std::endl;
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

      class dwarf::compile_units_type const &cus = dw.compile_units ();
      for (dwarf::compile_units_type::const_iterator it = cus.begin ();
	   it != cus.end (); ++it)
	recursively_validate (*it, *it);
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      wr_error (section_locus (sec_info))
	<< "Exception while checking expected trees: " << exc.what ()
	<< std::endl;
      throw check_base::failed ();
    }
}
