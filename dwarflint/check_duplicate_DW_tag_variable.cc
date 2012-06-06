/*
   Copyright (C) 2010, 2011 Red Hat, Inc.
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
#include <map>

using elfutils::dwarf;

namespace
{
  class check_duplicate_DW_tag_variable
    : public die_check
  {
    struct varinfo
    {
      dwarf::debug_info_entry::children_type::const_iterator decl, def;

      explicit varinfo (dwarf::debug_info_entry::children_type const &children)
	: decl (children.end ())
	, def (children.end ())
      {}

      varinfo (varinfo const &other)
	: decl (other.decl)
	, def (other.def)
      {}
    };

  public:
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("check_duplicate_DW_tag_variable")
	 .description (
"Implements a check for two full DW_TAG_variable DIEs with the same "
"DW_AT_name value.  This covers duplicate declaration, duplicate "
"definition and declaration with definition.\n"
" https://fedorahosted.org/pipermail/elfutils-devel/2010-July/001497.html\n"
" http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39524\n"));
      return &cd;
    }

    check_duplicate_DW_tag_variable (highlevel_check_i *,
				     checkstack &, dwarflint &) {}

    virtual void
    die (all_dies_iterator<dwarf> const &it)
    {
      dwarf::debug_info_entry::children_type const &children
	= it->children ();

      typedef std::map<std::string, varinfo> variables_map;
      variables_map variables;

      for (dwarf::debug_info_entry::children_type::const_iterator
	     jt = children.begin (); jt != children.end (); ++jt)
	{
	  if (jt->tag () == DW_TAG_variable)
	    {
	      dwarf::debug_info_entry::attributes_type const &
		attrs = jt->attributes ();
	      dwarf::debug_info_entry::attributes_type::const_iterator
		at, et = attrs.end ();
	      if ((at = attrs.find (DW_AT_name)) == et)
		continue;
	      char const *cname = (*at).second.identifier ();

	      bool declaration = false;
	      if ((at = attrs.find (DW_AT_declaration)) != et)
		declaration = (*at).second.flag ();

	      std::string name (cname);
	      variables_map::iterator old = variables.find (name);
	      if (old == variables.end ())
		{
		  varinfo i (children);
		  if (declaration)
		    i.decl = jt;
		  else
		    i.def = jt;
		  variables.insert (std::make_pair (name, i));
		}
	      else
		{
		  varinfo &i = old->second;
		  if ((declaration && i.decl != children.end ())
		      || (!declaration && i.def != children.end ()))
		    wr_message (die_locus (*jt), mc_impact_3 | mc_die_other)
		      .id (descriptor ())
		      << "Re" << (declaration ? "declaration" : "definition")
		      << " of variable '" << name << "', originally seen at "
		      << pri::ref (declaration ? *i.decl : *i.def)
		      << '.' << std::endl;
		  else
		    wr_message (die_locus (*jt), mc_impact_3 | mc_die_other)
		      .id (descriptor ())
		      << "Found "
		      << (declaration ? "declaration" : "definition")
		      << " of variable '" << name
		      << "' whose "
		      << (declaration ? "definition" : "declaration")
		      << " was seen at "
		      << pri::ref (declaration ? *i.def : *i.decl)
		      << '.' << std::endl;
		}
	    }
	}
    }
  };
  reg_die_check<check_duplicate_DW_tag_variable> reg;
}
