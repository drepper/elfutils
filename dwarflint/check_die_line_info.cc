/* Check that every block that has an address is also in line info table.
   Copyright (C) 2011 Red Hat, Inc.
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

#include "check_die_tree.hh"
#include "pri.hh"
#include "messages.hh"

using elfutils::dwarf;

namespace
{
  class check_die_line_info
    : public die_check
  {
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create ("check_die_line_info")
	 .description ("Check that each code block start address "
		       "is also mentioned in the line table.\n"));
      return &cd;
    }

    check_die_line_info (highlevel_check_i *, checkstack &, dwarflint &)
    {
      // No state stored for this check.
    }

    static bool is_code_block (dwarf::debug_info_entry const &entry)
    {
      int tag = entry.tag ();
      switch (tag)
	{
	case DW_TAG_subprogram:
	case DW_TAG_inlined_subroutine:
	case DW_TAG_entry_point:
	case DW_TAG_lexical_block:
	case DW_TAG_label:
	case DW_TAG_with_stmt:
	case DW_TAG_try_block:
	case DW_TAG_catch_block:
	  return true;

	default:
	  return false;
	}
    }

    static void check_die_pc (dwarf::debug_info_entry const &entry,
			      dwarf::attribute const &pc,
			      Dwarf_Addr addr)
    {
      dwarf::compile_unit cu = entry.compile_unit ();
      dwarf::line_info_table line_info = cu.line_info ();
      dwarf::line_table lines = line_info.lines ();
      dwarf::line_table::const_iterator l = lines.find (addr);

      bool found = false;
      while (l != lines.end ())
	{
	  dwarf::line_entry line = *l;
	  if (line.address () < addr)
	    {
	      l++;
	      continue;
	    }
	  else if (line.address () > addr)
	    {
	      // Ran past it...
	      break;
	    }

	  found = true;
	  break;
	}

      if (! found)
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " " << dwarf::attributes::name (pc.first) << "=0x"
	  << std::hex << addr << std::dec
	  << ", NOT found in line table." << std::endl;
    }

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      dwarf::debug_info_entry const &entry = *it;
      if (is_code_block (entry))
	{
	  dwarf::debug_info_entry::attributes_type attrs = entry.attributes ();

	  dwarf::debug_info_entry::attributes_type::const_iterator
	    entry_pc = attrs.find (DW_AT_entry_pc);
	  if (entry_pc != attrs.end ())
	    check_die_pc (entry, *entry_pc, (*entry_pc).second.address ());

	  dwarf::debug_info_entry::attributes_type::const_iterator
	    low_pc = attrs.find (DW_AT_low_pc);
	  if (low_pc != attrs.end ())
	    check_die_pc (entry, *low_pc, (*low_pc).second.address ());

	  dwarf::debug_info_entry::attributes_type::const_iterator
	    at_ranges = attrs.find (DW_AT_ranges);
	  if (at_ranges != attrs.end ())
	    {
	      dwarf::ranges ranges = entry.ranges ();
	      dwarf::ranges::const_iterator r = ranges.begin ();
	      while (r != ranges.end ())
		{
		  check_die_pc (entry, *at_ranges, (*r).first);
		  r++;
		}
	    }
	}
    }
  };

  reg_die_check<check_die_line_info> reg;
}
