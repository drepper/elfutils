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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "highlevel_check.hh"
#include "../src/dwarfstrings.h"
#include "all-dies-it.hh"
#include "pri.hh"
#include "messages.h"
#include <map>

using elfutils::dwarf;

namespace
{
  class check_duplicate_DW_tag_variable
    : public highlevel_check<check_duplicate_DW_tag_variable>
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
    static checkdescriptor descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("check_duplicate_DW_tag_variable")
	 .description (
"Implements a check for two full DW_TAG_variable DIEs with the same\n"
"DW_AT_name value.  This covers duplicate declaration, duplicate\n"
"definition and declaration with definition.\n"
" https://fedorahosted.org/pipermail/elfutils-devel/2010-July/001497.html\n"
" http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39524\n"));
      return cd;
    }

    check_duplicate_DW_tag_variable (checkstack &stack, dwarflint &lint);
  };

  reg<check_duplicate_DW_tag_variable> reg_duplicate_DW_tag_variable;
}

check_duplicate_DW_tag_variable
::check_duplicate_DW_tag_variable (checkstack &stack, dwarflint &lint)
  : highlevel_check<check_duplicate_DW_tag_variable> (stack, lint)
{
  for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
       it != all_dies_iterator<dwarf> (); ++it)
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
		    wr_error (to_where (*jt))
		      << "Re" << (declaration ? "declaration" : "definition")
		      << " of variable '" << name << "', originally seen at "
		      << pri::ref (declaration ? *i.decl : *i.def)
		      << '.' << std::endl;
		  else
		    wr_error (to_where (*jt))
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
}
