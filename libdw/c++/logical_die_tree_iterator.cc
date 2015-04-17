/* -*-c++-*-
   Copyright (C) 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>

#include "libdw"
#include "libdwP.hh"

namespace
{
  bool
  advance (std::pair <elfutils::v1::die_tree_iterator,
		      elfutils::v1::die_tree_iterator> &p)
  {
    return ++p.first != p.second;
  }
}

attribute_hidden
bool
elfutils::v1::logical_die_tree_iterator::move ()
{
  while (true)
    if (m_stack.empty ())
      return false;
    else if (advance (m_stack.back ()))
      {
	Dwarf_Die &die = **this;
	Dwarf_Attribute at_import;
	Dwarf_Die cudie;
	if (dwarf_tag (&die) == DW_TAG_imported_unit
	    && dwarf_hasattr (&die, DW_AT_import)
	    && dwarf_attr (&die, DW_AT_import, &at_import) != NULL
	    && dwarf_formref_die (&at_import, &cudie) != NULL)
	  {
	    unit_iterator uit_end (dwarf_cu_getdwarf (cudie.cu), cudie);
	    unit_iterator uit = uit_end++;
	    m_stack.push_back (std::make_pair (die_tree_iterator (uit),
					       die_tree_iterator (uit_end)));
	    // Now m_stack.back() references a CU DIE.  Go around once
	    // more to advance to the first child.  If there is none,
	    // it will get popped again and the whole process continues.
	  }
	else
	  return true;
      }
    else
      m_stack.pop_back ();
}

elfutils::v1::logical_die_tree_iterator::logical_die_tree_iterator (end_it)
{}

elfutils::v1::logical_die_tree_iterator::logical_die_tree_iterator (Dwarf *dw)
{
  die_tree_iterator it = die_tree_iterator (dw);
  if (it != die_tree_iterator::end ())
    m_stack.push_back (std::make_pair (it, die_tree_iterator::end ()));
}

elfutils::v1::logical_die_tree_iterator
	::logical_die_tree_iterator (unit_iterator const &cuit)
{
  die_tree_iterator it = die_tree_iterator (cuit);
  if (it != die_tree_iterator::end ())
    m_stack.push_back (std::make_pair (it, die_tree_iterator::end ()));
}

elfutils::v1::logical_die_tree_iterator
elfutils::v1::logical_die_tree_iterator::end ()
{
  return logical_die_tree_iterator (end_it ());
}

bool
elfutils::v1::logical_die_tree_iterator
	::operator== (logical_die_tree_iterator const &that) const
{
  return m_stack == that.m_stack;
}

bool
elfutils::v1::logical_die_tree_iterator
	::operator!= (logical_die_tree_iterator const &that) const
{
  return ! (*this == that);
}


elfutils::v1::logical_die_tree_iterator &
elfutils::v1::logical_die_tree_iterator::operator++ ()
{
  assert (! m_stack.empty ());

  if (! move ())
    *this = end ();

  return *this;
}

elfutils::v1::logical_die_tree_iterator
elfutils::v1::logical_die_tree_iterator::operator++ (int)
{
  logical_die_tree_iterator ret = *this;
  ++*this;
  return ret;
}

Dwarf_Die &
elfutils::v1::logical_die_tree_iterator::operator* ()
{
  assert (! m_stack.empty ());
  return *m_stack.back ().first;
}

Dwarf_Die *
elfutils::v1::logical_die_tree_iterator::operator-> ()
{
  return &**this;
}

elfutils::v1::logical_die_tree_iterator
elfutils::v1::logical_die_tree_iterator::parent ()
{
  // XXX
  assert (! "implement me");
}
