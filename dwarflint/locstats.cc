/*
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

#include "highlevel_check.hh"
#include "all-dies-it.hh"
#include "option.hh"

#include <sstream>

using elfutils::dwarf;

namespace
{
  class locstats
    : public highlevel_check<locstats>
  {
  public:
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("locstats")
	 .groups ("@nodefault"));
      return &cd;
    }

    locstats (checkstack &stack, dwarflint &lint);
  };

  reg<locstats> reg_locstats;

  void_option ignore_single_addr
    ("Exclude global and static variables from the statistics.",
     "locstats:ignore-single-addr");

  string_option tabulation_rule
    ("Rule for sorting results into buckets.",
     "start[:step][,...]",
     "locstats:tabulate");

  struct tabrule
  {
    int start;
    int step;
    tabrule (int a_start, int a_step)
      : start (a_start), step (a_step)
    {}
    bool operator < (tabrule const &other) const {
      return start < other.start;
    }
  };

  struct tabrules_t
    : public std::vector<tabrule>
  {
    tabrules_t (std::string const &rule)
    {
      std::stringstream ss;
      ss << rule;

      std::string item;
      while (std::getline (ss, item, ','))
	{
	  if (item.empty ())
	    continue;
	  int start;
	  int step;
	  int r = std::sscanf (item.c_str (), "%d:%d", &start, &step);
	  if (r == EOF || r == 0)
	    continue;
	  if (r == 1)
	    step = 0;
	  push_back (tabrule (start, step));
	}

      push_back (tabrule (100, 0));
      std::sort (begin (), end ());
    }

    void next ()
    {
      if (at (0).step == 0)
	erase (begin ());
      else
	{
	  at (0).start += at (0).step;
	  if (size () > 1)
	    {
	      if (at (0).start > at (1).start)
		erase (begin ());
	      while (size () > 1
		     && at (0).start == at (1).start)
		erase (begin ());
	    }
	}
    }

    bool match (int value) const
    {
      return at (0).start == value;
    }
  };
}

locstats::locstats (checkstack &stack, dwarflint &lint)
  : highlevel_check<locstats> (stack, lint)
{
  std::map<int, unsigned long> tally;
  unsigned long total = 0;
  for (int i = 0; i <= 100; ++i)
    tally[i] = 0;

  tabrules_t tabrules (tabulation_rule.seen ()
		       ? tabulation_rule.value () : "10:10");

  for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
       it != all_dies_iterator<dwarf> (); ++it)
    {
      dwarf::debug_info_entry const &die = *it;

      // We are interested in variables and formal parameters
      bool is_formal_parameter = die.tag () == DW_TAG_formal_parameter;
      if (!is_formal_parameter && die.tag () != DW_TAG_variable)
	continue;

      dwarf::debug_info_entry const &parent = it.parent ();

      dwarf::debug_info_entry::attributes_type const &attrs
	= die.attributes ();

      // ... except those that are just declarations
      if (attrs.find (DW_AT_declaration) != attrs.end ())
	continue;

      // Of formal parameters we ignore those that are children of
      // subprograms that are themselves declarations.
      if (is_formal_parameter)
	{
	  if (parent.attributes ().find (DW_AT_declaration)
	      != die.attributes ().end ())
	    continue;
	}

      // Unfortunately the location expression is not yet wrapped
      // in c++, so we need to revert back to C code.
      Dwarf_Die die_c_mem,
	*die_c = dwarf_offdie (this->c_dw, die.offset (), &die_c_mem);
      assert (die_c != NULL);

      Dwarf_Attribute locattr_mem,
	*locattr = dwarf_attr_integrate (die_c, DW_AT_location, &locattr_mem);

      // Also ignore extern globals -- these have DW_AT_external and
      // no DW_AT_location.
      if (attrs.find (DW_AT_external) != attrs.end () && locattr == NULL)
	continue;

      /*
      Dwarf_Attribute name_attr_mem,
	*name_attr = dwarf_attr_integrate (die_c, DW_AT_name, &name_attr_mem);
      std::string name = name_attr != NULL
	? dwarf_formstring (name_attr)
	: (dwarf_hasattr_integrate (die_c, DW_AT_artificial)
	   ? "<artificial>" : "???");

      std::cerr << "die=" << std::hex << die.offset ()
		<< " '" << name << '\'';
      */

      int coverage;
      Dwarf_Op *expr;
      size_t len;

      // consts need no location
      if (attrs.find (DW_AT_const_value) != attrs.end ())
	coverage = 100;

      // no location
      else if (locattr == NULL)
	coverage = 0;

      // non-list location
      else if (dwarf_getlocation (locattr, &expr, &len) == 0)
	{
	  if (len == 1 && expr[0].atom == DW_OP_addr
	      && ignore_single_addr)
	    // Globals and statics have non-list location that is a
	    // singleton DW_OP_addr expression.
	    continue;
	  coverage = 100;
	}

      // location list
      else
	{
	  dwarf::ranges const &ranges
	    = die.ranges ().empty () ? parent.ranges () : die.ranges ();
	  if (ranges.empty ())
	    // what?
	    continue;

	  size_t length = 0;
	  size_t covered = 0;
	  for (dwarf::ranges::const_iterator rit = ranges.begin ();
	       rit != ranges.end (); ++rit)
	    {
	      Dwarf_Addr low = (*rit).first;
	      Dwarf_Addr high = (*rit).second;
	      length += high - low;
	      /*std::cerr << std::endl << " " << low << ".." << high
		<< std::endl;*/
	      for (Dwarf_Addr addr = low; addr < high; ++addr)
		if (dwarf_getlocation_addr (locattr, addr,
					    NULL, NULL, 0) > 0)
		  covered++;
	    }
	  coverage = 100 * covered / length;
	}

      tally[coverage]++;
      total++;
    }

  unsigned long cumulative = 0;
  unsigned long last = 0;
  int last_pct = 0;
  std::cout << "cov%\tsamples\tcumul" << std::endl;
  for (int i = 0; i <= 100; ++i)
    {
      cumulative += tally.find (i)->second;
      if (tabrules.match (i))
	{
	  long int samples = cumulative - last;
	  std::cout << std::dec << last_pct;
	  if (last_pct != i)
	    std::cout << ".." << i;
	  std::cout << "\t" << samples
		    << '/' << (100*samples / total) << '%'
		    << "\t" << cumulative
		    << '/' << (100*cumulative / total) << '%'
		    << std::endl;
	  last = cumulative;
	  last_pct = i + 1;

	  tabrules.next ();
	}
    }
}
