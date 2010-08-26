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

#include "checks.hh"
#include "options.h"
#include <sstream>

namespace
{
  std::vector<std::string>
  split_groups (std::string const &str)
  {
    std::stringstream ss (str);
    std::string item;
    std::vector<std::string> ret;

    while (ss >> item)
      ret.push_back (item);

    return ret;
  }
}

checkdescriptor::checkdescriptor (std::string const &desc)
  : groups (::split_groups (desc))
  , name (groups[0])
{
  groups.erase (groups.begin ());
}


reporter::reporter (checkstack const &s, checkdescriptor const &a_cd)
  : stack (s)
  , cd (a_cd)
{
  (*this) ("...", true);
}

void
reporter::operator () (char const *what, bool ext)
{
  if (!be_verbose)
    return;

  if (false)
    for (size_t i = 0; i < stack.size (); ++i)
      std::cout << ' ';

  std::cout << cd.name << ' ' << what;
  if (ext)
    {
      std::cout << " [";
      for (std::vector<std::string>::const_iterator it = cd.groups.begin ();
	   it != cd.groups.end (); ++it)
	{
	  if (it != cd.groups.begin ())
	    std::cout << ',';
	  std::cout << *it;
	}
      std::cout << "] {";
      for (checkstack::const_iterator it = stack.begin ();
	   it != stack.end (); ++it)
	{
	  if (it != stack.begin ())
	    std::cout << ',';
	  std::cout << (*it)->name;
	}
      std::cout << "}";
    }
  std::cout << std::endl;
}
