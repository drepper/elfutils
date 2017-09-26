/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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

#include "check_die_tree.hh"
#include "pri.hh"
#include "messages.hh"
#include <map>

using elfutils::dwarf;

namespace
{
  class check_dups_abstract_origin
    : public die_check
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
    duplicate_ok (int tag, int at, int from, int ref_tag, bool same)
    {
      // A call site entry has a DW_AT_low_pc attribute which is the return
      // address after the call and a DW_AT_abstract_origin that is a
      // pointer to the reference it calls directly or indirectly. So
      // both may be available also at the abstract_origin (with different
      // values).
      if (tag == DW_TAG_GNU_call_site
	  && (at == DW_AT_low_pc || at == DW_AT_abstract_origin)
	  && from == DW_AT_abstract_origin
	  && ! same)
	return true;

      // A subprogram that has a concrete out-of-line instance might
      // have an object_pointer different from the original variant
      // of the subprogram. Similar for a subprogram specification,
      // which may refer to the specification die of the object_pointer,
      // while the instance of the subprogram will refer to the
      // actual instance of the object_pointer die.
      if (tag == DW_TAG_subprogram
	  && at == DW_AT_object_pointer
	  && (from == DW_AT_abstract_origin || from == DW_AT_specification)
	  && ref_tag == DW_TAG_subprogram
	  && ! same)
	return true;

      // A subprogram can be defined outside the body of the enclosing
      // class, then file and/or line attributes can differ.
      if (tag == DW_TAG_subprogram
	  && from == DW_AT_specification
	  && (at == DW_AT_decl_line || at == DW_AT_decl_file)
	  && ref_tag == DW_TAG_subprogram
	  && ! same)
	return true;

      // Same for a member variable can be defined outside the body of the
      // enclosing class, then file and/or line attributes can differ.
      if (tag == DW_TAG_variable
	  && from == DW_AT_specification
	  && (at == DW_AT_decl_line || at == DW_AT_decl_file)
	  && ref_tag == DW_TAG_member
	  && ! same)
	return true;


      return false;
    }

    void
    check_die_attr (dwarf::debug_info_entry const &entry,
		    dwarf::attribute const &attr)
    {
      std::map<unsigned int, dwarf::attr_value> m;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = entry.attributes ().begin ();
	   at != entry.attributes ().end (); ++at)
	m.insert (std::make_pair ((*at).first, (*at).second));

      dwarf::attr_value const &val = attr.second;
      // xxx Referree can't be const&, gives memory errors.
      dwarf::debug_info_entry referree = *val.reference ();

      std::map<unsigned int, dwarf::attr_value>::const_iterator at2;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = referree.attributes ().begin ();
	   at != referree.attributes ().end (); ++at)
	if ((at2 = m.find ((*at).first)) != m.end ()
	    && ! duplicate_ok (entry.tag (), at2->first, attr.first,
			       referree.tag (), at2->second == (*at).second))
	  wr_message (die_locus (entry), mc_impact_3 | mc_acc_bloat | mc_die_rel)
	    .id (descriptor ())
	    << dwarf::tags::name (entry.tag ())
	    << " attribute " << dwarf::attributes::name (at2->first)
	    << " is duplicated at " << dwarf::attributes::name (attr.first)
	    << " (" << pri::ref (referree) << ")"
	    << (at2->second == (*at).second
		? "." : " with different value.")
	    << std::endl;
    }

    explicit
    check_dups_abstract_origin (highlevel_check_i *, checkstack &, dwarflint &)
    {
      // No state necessary.
    }

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      // Do we have DW_AT_abstract_origin or DW_AT_specification?
      dwarf::debug_info_entry const &entry = *it;
      for (dwarf::debug_info_entry::attributes_type::const_iterator
	     at = entry.attributes ().begin ();
	   at != entry.attributes ().end (); ++at)
	if ((*at).first == DW_AT_abstract_origin
	    || (*at).first == DW_AT_specification)
	  {
	    assert ((*at).second.what_space () == dwarf::VS_reference);
	    check_die_attr (entry, *at);
	  }
    }
  };

  reg_die_check<check_dups_abstract_origin> reg;
}
