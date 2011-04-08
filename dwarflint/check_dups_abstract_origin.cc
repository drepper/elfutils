/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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
#include "all-dies-it.hh"
#include "pri.hh"
#include "messages.hh"
#include <map>

using elfutils::dwarf;

namespace
{
  class check_dups_abstract_origin
    : public highlevel_check<check_dups_abstract_origin>
  {
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create ("check_dups_abstract_origin")
	 .description (
"If a given attribute name is present on a DIE, it is "
"suspicious if that attribute name appears on the DIE that's the "
"first DIE's DW_AT_abstract_origin or DW_AT_specification.\n"
" https://bugzilla.redhat.com/show_bug.cgi?id=527430\n"));
      return &cd;
    }

    bool
    duplicate_ok (int tag, int at, int from)
    {
      // A call site entry has a DW_AT_low_pc attribute which is the return
      // address after the call and a DW_AT_abstract_origin that is a
      // pointer to the reference it calls directly or indirectly. So
      // both may be available also at the abstract_origin (with different
      // values).
      if (tag == DW_TAG_GNU_call_site
	  && (at == DW_AT_low_pc || at == DW_AT_abstract_origin)
	  && from == DW_AT_abstract_origin)
	return true;

      return false;
    }

    void
    check_die_attr (dwarf::debug_info_entry const &die,
		    dwarf::attribute const &attr)
    {
      std::map<unsigned int, dwarf::attr_value> m;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = die.attributes ().begin ();
	   at != die.attributes ().end (); ++at)
	m.insert (std::make_pair ((*at).first, (*at).second));

      dwarf::attr_value const &val = attr.second;
      // xxx Referree can't be const&, gives memory errors.
      dwarf::debug_info_entry referree = *val.reference ();

      std::map<unsigned int, dwarf::attr_value>::const_iterator at2;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = referree.attributes ().begin ();
	   at != referree.attributes ().end (); ++at)
	if ((at2 = m.find ((*at).first)) != m.end ()
	    && ! duplicate_ok (die.tag (), at2->first, attr.first))
	  wr_message (to_where (die), mc_impact_3 | mc_acc_bloat | mc_die_rel)
	    << "Attribute " << dwarf::attributes::name (at2->first)
	    << " is duplicated at " << dwarf::attributes::name (attr.first)
	    << " (" << pri::ref (referree) << ")"
	    << (at2->second == (*at).second
		? "." : " with different value.")
	    << std::endl;
    }

    explicit check_dups_abstract_origin (checkstack &stack, dwarflint &lint)
      : highlevel_check<check_dups_abstract_origin> (stack, lint)
    {
      for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
	   it != all_dies_iterator<dwarf> (); ++it)
	{
	  // Do we have DW_AT_abstract_origin or DW_AT_specification?
	  dwarf::debug_info_entry const &die = *it;
	  for (dwarf::debug_info_entry::attributes_type::const_iterator
		 at = die.attributes ().begin ();
	       at != die.attributes ().end (); ++at)
	    if ((*at).first == DW_AT_abstract_origin
		|| (*at).first == DW_AT_specification)
	      {
		assert ((*at).second.what_space () == dwarf::VS_reference);
		check_die_attr (die, *at);
	      }
	}
    }
  };

  reg<check_dups_abstract_origin> reg;
}
