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

#include "checkdescriptor.hh"
#include <sstream>
#include <cassert>

std::ostream &
operator << (std::ostream &o, checkgroups const &groups)
{
  o << '[';
  for (checkgroups::const_iterator it = groups.begin ();
       it != groups.end (); ++it)
    {
      if (it != groups.begin ())
	o << ',';
      o << *it;
    }
  o << ']';
  return o;
}

std::ostream &
operator << (std::ostream &o, prereqs const &p)
{
  o << "(";
  for (prereqs::const_iterator it = p.begin (); it != p.end (); ++it)
    {
      if (it != p.begin ())
	o << ',';
      o << (*it)->name ();
    }
  o << ")";
  return o;
}

checkdescriptor::create::create (char const *name)
  : _m_name (name)
  , _m_description (NULL)
  , _m_hidden (false)
{}

checkdescriptor::create &
checkdescriptor::create::groups (char const *a_groups)
{
  std::stringstream ss (a_groups);
  std::string group;
  while (ss >> group)
    _m_groups.insert (group);
  return *this;
}

checkdescriptor::checkdescriptor (create const &c)
  : _m_name (c._m_name)
  , _m_description (c._m_description)
  , _m_groups (c._m_groups)
  , _m_prereq (c._m_prereq)
  , _m_hidden (c._m_hidden)
  , _m_opts (c._m_opts)
{}

bool
checkdescriptor::in_group (std::string const &group) const
{
  return _m_groups.find (group) != _m_groups.end ();
}
