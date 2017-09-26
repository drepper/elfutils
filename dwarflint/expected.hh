/* Pedantic checking of DWARF files.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2009.

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

#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <cassert>

#include "../libdw/c++/dwarf"

enum optionality
{
  opt_optional = 0,	// may or may not be present
  opt_required,		// bogus if missing
  opt_expected,		// suspicious if missing
};

template <class T>
std::string
string_of (T x)
{
  std::ostringstream o;
  o << x;
  return o.str();
}

struct expected_set
{
  typedef std::map <int, optionality> expectation_map;

private:
  expectation_map m_map;

public:
#define DEF_FILLER(WHAT)						\
  expected_set &WHAT (int attribute)					\
  {									\
    assert (m_map.find (attribute) == m_map.end ());			\
    m_map.insert (std::make_pair (attribute, opt_##WHAT));		\
    return *this;							\
  }									\
  expected_set &WHAT (std::set <int> const &attributes)			\
  {									\
    for (std::set <int>::const_iterator it = attributes.begin ();	\
	 it != attributes.end (); ++it)					\
      WHAT (*it);							\
    return *this;							\
  }

  DEF_FILLER (required)
  DEF_FILLER (expected)
  DEF_FILLER (optional)
#undef DEF_FILLER

  expectation_map const &map () const
  {
    return m_map;
  }
};

class expected_map
{
  typedef std::map <int, expected_set> expected_map_t;

protected:
  expected_map_t m_map;
  expected_map () {}

public:
  expected_set::expectation_map const &map (int tag) const
  {
    expected_map_t::const_iterator it = m_map.find (tag);
    if (it == m_map.end ())
      throw std::runtime_error ("Unknown tag "
				+ elfutils::dwarf::tags::identifier (tag));
    return it->second.map ();
  }
};

struct expected_at_map
  : public expected_map
{
  expected_at_map ();
};
