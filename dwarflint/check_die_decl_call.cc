/* Check that decl or call file, line, column come in pairs.
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
  class check_die_decl_call
    : public die_check
  {
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create ("check_die_decl_call")
	 .description ("Check that each decl or call attribute come in"
		       " file/line and column/line pairs.\n"));
      return &cd;
    }

    check_die_decl_call (highlevel_check_i *, checkstack &, dwarflint &)
    {
      // No state stored for this check.
    }

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      dwarf::debug_info_entry const &entry = *it;
      dwarf::debug_info_entry::attributes_type attrs = entry.attributes ();

      // Make sure decl column and line, and file and line are paired.
      dwarf::debug_info_entry::attributes_type::const_iterator
	decl_column = attrs.find (DW_AT_decl_column);
      dwarf::debug_info_entry::attributes_type::const_iterator
	decl_line = attrs.find (DW_AT_decl_line);
      dwarf::debug_info_entry::attributes_type::const_iterator
	decl_file = attrs.find (DW_AT_decl_file);

      if (decl_column != attrs.end () && decl_line == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has decl_column, but NOT decl_line" << std::endl;

      if (decl_line != attrs.end () && decl_file == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has decl_line, but NOT decl_file" << std::endl;

      if (decl_file != attrs.end () && decl_line == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has decl_file, but NOT decl_line" << std::endl;

      // Same for call variants.
      dwarf::debug_info_entry::attributes_type::const_iterator
	call_column = attrs.find (DW_AT_call_column);
      dwarf::debug_info_entry::attributes_type::const_iterator
	call_line = attrs.find (DW_AT_call_line);
      dwarf::debug_info_entry::attributes_type::const_iterator
	call_file = attrs.find (DW_AT_call_file);

      if (call_column != attrs.end () && call_line == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has call_column, but NOT call_line" << std::endl;

      if (call_line != attrs.end () && call_file == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has call_line, but NOT call_file" << std::endl;

      if (call_file != attrs.end () && call_line == attrs.end ())
	wr_message (die_locus (entry), mc_impact_3 | mc_acc_suboptimal)
	  .id (descriptor ())
	  << elfutils::dwarf::tags::name (entry.tag ())
	  << " has call_file, but NOT call_line" << std::endl;
    }
  };

  reg_die_check<check_die_decl_call> reg;
}
