/* Check that every die that has a linkage_name is also external.
   Copyright (C) 2011 Red Hat, Inc.
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
#include "../src/dwarfstrings.h"
#include "all-dies-it.hh"
#include "pri.hh"
#include "messages.hh"
#include <map>

using elfutils::dwarf;

namespace
{
  class check_linkage_external_die
    : public highlevel_check<check_linkage_external_die>
  {
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create ("check_linkage_external_die")
	 .inherit<highlevel_check<check_linkage_external_die> > ()
	 .description (
"Check that each DIE that has a linkage_name also has an external attribute.\n"
		       ));
      return &cd;
    }

    explicit check_linkage_external_die (checkstack &stack, dwarflint &lint)
      : highlevel_check<check_linkage_external_die> (stack, lint)
    {
      for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
	   it != all_dies_iterator<dwarf> (); ++it)
	{
	  dwarf::debug_info_entry const &die = *it;
	  dwarf::debug_info_entry::attributes_type attrs = die.attributes ();
	  if ((attrs.find (DW_AT_linkage_name) != attrs.end ()
	       || attrs.find (DW_AT_MIPS_linkage_name) != attrs.end ())
	      && attrs.find (DW_AT_external) == attrs.end ())
	    {
	      wr_message (to_where (die),
			  mc_impact_3 | mc_acc_suboptimal | mc_die_other)
		<< elfutils::dwarf::tags::name (die.tag ())
		<< " has linkage_name attribute, but no external attribute."
		<< std::endl;
	    }
	}
    }
  };

  reg<check_linkage_external_die> reg;
}
