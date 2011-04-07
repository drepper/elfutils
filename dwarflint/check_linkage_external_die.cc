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

#include "check_die_tree.hh"
#include "pri.hh"
#include "messages.hh"

using elfutils::dwarf;

namespace
{
  class check_linkage_external_die
    : public die_check
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

    check_linkage_external_die (highlevel_check_i *, checkstack &, dwarflint &)
    {
      // We don't keep any state for this die check.
    }

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      dwarf::debug_info_entry const &entry = *it;
      dwarf::debug_info_entry::attributes_type attrs = entry.attributes ();
      if ((attrs.find (DW_AT_linkage_name) != attrs.end ()
	   || attrs.find (DW_AT_MIPS_linkage_name) != attrs.end ())
	  && attrs.find (DW_AT_external) == attrs.end ())
	{
	  wr_message (to_where (entry),
		      mc_impact_3 | mc_acc_suboptimal | mc_die_other)
	    .id (descriptor ())
	    << elfutils::dwarf::tags::name (entry.tag ())
	    << " has linkage_name attribute, but no external attribute."
	    << std::endl;
	}
    }
  };

  reg_die_check<check_linkage_external_die> reg;
}
