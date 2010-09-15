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
#if 0
  std::cout << "---\nstack" << std::endl;
  for (checkstack::const_iterator jt = stack.begin ();
       jt != stack.end (); ++jt)
    std::cout << (*jt)->name << std::flush << "  ";
  std::cout << std::endl;
#endif

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
