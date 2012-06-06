/*
   Copyright (C) 2010 Red Hat, Inc.
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

#include "checkrule.hh"
#include "checkdescriptor.hh"
#include "dwarflint.hh"
#include <cassert>

checkrule::checkrule (std::string const &a_name, action_t an_action)
  : _m_name (a_name)
  , _m_action (an_action)
  , _m_used (false)
{
}

checkrule_internal::checkrule_internal (std::string const &a_name,
					action_t an_action)
  : checkrule (a_name, an_action)
{
  mark_used ();
}

namespace
{
  bool
  rule_matches (std::string const &name,
		checkdescriptor const &cd)
  {
    if (name == "@all")
      return true;
    if (name == "@none")
      return false;
    if (name == cd.name ())
      return true;
    return cd.in_group (name);
  }
}

bool
checkrules::should_check (checkstack const &stack) const
{
  // We always allow scheduling hidden checks.  Those are service
  // routines that the user doesn't even see it the list of checks.
  assert (!stack.empty ());
  if (stack.back ()->hidden ())
    return true;

  bool should = false;
  for (const_iterator it = begin (); it != end (); ++it)
    {
      std::string const &rule_name = it->name ();
      bool nflag = it->action () == checkrule::request;
      if (nflag == should && it->used ())
	continue;

      for (checkstack::const_iterator jt = stack.begin ();
	   jt != stack.end (); ++jt)
	if (rule_matches (rule_name, **jt))
	  {
	    it->mark_used ();
	    //std::cout << " rule: " << rule_name << " " << nflag << std::endl;
	    should = nflag;
	    break;
	  }
    }

  return should;
}
