/* Check for DIEs with attributes referencing the DIE itself.
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
  class check_self_referential_die
    : public die_check
  {
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create ("check_self_referential_die")
	 .description (
"A reference attribute referencing the DIE itself is suspicious.\n"
"One example is a DW_AT_containing_type pointing to itself.\n"
" https://fedorahosted.org/pipermail/elfutils-devel/2011-February/001794.html\n"
		       ));
      return &cd;
    }

    check_self_referential_die (highlevel_check_i *, checkstack &, dwarflint &)
    {
      // We don't keep any state for this die check.
    }

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      dwarf::debug_info_entry const &entry = *it;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = entry.attributes ().begin ();
	   at != entry.attributes ().end (); ++at)
	{
	  dwarf::attr_value const &val = (*at).second;
	  if (val.what_space () == dwarf::VS_reference)
	    {
	      dwarf::debug_info_entry ref = *val.reference ();
	      if (ref.identity () == entry.identity ())
		wr_message (die_locus (entry),
			    mc_impact_3 | mc_acc_suboptimal | mc_die_rel)
		  .id (descriptor ())
		  << dwarf::tags::name (entry.tag ())
		  << " attribute " << dwarf::attributes::name ((*at).first)
		  << " references DIE itself." << std::endl;
	    }
	}
    }
  };

  reg_die_check<check_self_referential_die> reg;
}
