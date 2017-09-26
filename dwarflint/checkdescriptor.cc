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

#include "checkdescriptor.hh"
#include "wrap.hh"
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

checkdescriptor::create::create (char const *name)
  : _m_name (name)
  , _m_description (NULL)
  , _m_hidden (false)
  , _m_schedule (true)
{}

checkdescriptor::create::create (checkdescriptor const &base)
  : _m_groups (base.groups ())
  , _m_name (base.name ())
  , _m_description (base.description ())
  , _m_hidden (base.hidden ())
  , _m_schedule (base.schedule ())
  , _m_opts (base.opts ())
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

checkdescriptor::checkdescriptor ()
  : _m_name (NULL)
  , _m_description (NULL)
  , _m_groups ()
  , _m_hidden (false)
  , _m_schedule (true)
  , _m_opts ()
{}

checkdescriptor::checkdescriptor (create const &c)
  : _m_name (c._m_name)
  , _m_description (c._m_description)
  , _m_groups (c._m_groups)
  , _m_hidden (c._m_hidden)
  , _m_schedule (c._m_schedule)
  , _m_opts (c._m_opts)
{}

bool
checkdescriptor::in_group (std::string const &group) const
{
  return _m_groups.find (group) != _m_groups.end ();
}

void
checkdescriptor::list (bool verbose) const
{
  const size_t columns = 70;

  if (verbose)
    std::cout << "=== " << name () << " ===";
  else
    std::cout << name ();

  checkgroups const &g = groups ();
  if (!g.empty ())
    {
      if (verbose)
	std::cout << std::endl << "groups: ";
      else
	std::cout << ' ';
      std::cout << g;
    }
  std::cout << std::endl;

  if (verbose)
    {
      char const *desc = description ();
      if (desc != NULL)
	std::cout << wrap_str (desc, columns).join ();

      options const &o = opts ();
      if (!o.empty ())
	{
	  std::cout << "recognized options:" << std::endl;
	  argp a = o.build_argp ();
	  argp_help (&a, stdout, ARGP_HELP_LONG, NULL);
	}

      std::cout << std::endl;
    }
}
