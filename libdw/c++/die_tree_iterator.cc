/* -*-c++-*-
   Copyright (C) 2009, 2010, 2011, 2012, 2014, 2015 Red Hat, Inc.
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
#include <algorithm>

#include "libdw"
#include "libdwP.hh"

namespace
{
  Dwarf_Die
  offdie (elfutils::v1::unit_iterator &cuit, Dwarf_Off offset)
  {
    return (dwarf_tag (&cuit->cudie) == DW_TAG_type_unit
	    ? dwpp_offdie_types : dwpp_offdie)
      (dwarf_cu_getdwarf (cuit->cudie.cu), offset);
  }
}

attribute_hidden
bool
elfutils::v1::die_tree_iterator::move ()
{
  Dwarf_Die child;
  if (dwpp_child (m_die, child))
    {
      m_stack.push_back (dwarf_dieoffset (&m_die));
      m_die = child;
      return true;
    }

  do
    if (dwpp_siblingof (m_die, m_die))
      return true;
    else
      // No sibling found.  Go a level up and retry, unless this
      // was a sole, childless CU DIE.
      if (! m_stack.empty ())
	{
	  m_die = offdie (m_cuit, m_stack.back ());
	  m_stack.pop_back ();
	}
  while (! m_stack.empty ());

  if (++m_cuit == elfutils::v1::unit_iterator::end ())
    return false;

  m_die = m_cuit->cudie;
  return true;
}

attribute_hidden
elfutils::v1::die_tree_iterator::die_tree_iterator (end_it)
  : m_cuit (unit_iterator::end ())
{}

elfutils::v1::die_tree_iterator::die_tree_iterator (Dwarf *dw)
  : m_cuit (unit_iterator (dw))
{
  if (m_cuit != unit_iterator::end ())
    m_die = m_cuit->cudie;
}

elfutils::v1::die_tree_iterator::die_tree_iterator (unit_iterator const &cuit)
  : m_cuit (cuit)
{
  if (m_cuit != unit_iterator::end ())
    m_die = m_cuit->cudie;
}

elfutils::v1::die_tree_iterator
elfutils::v1::die_tree_iterator::end ()
{
  return die_tree_iterator (end_it ());
}

bool
elfutils::v1::die_tree_iterator::operator== (die_tree_iterator
						const &that) const
{
  return m_cuit == that.m_cuit
    && (m_cuit == unit_iterator::end () || m_die.addr == that.m_die.addr);
}

bool
elfutils::v1::die_tree_iterator::operator!= (die_tree_iterator
						const &that) const
{
  return ! (*this == that);
}

elfutils::v1::die_tree_iterator &
elfutils::v1::die_tree_iterator::operator++ ()
{
  assert (m_cuit != unit_iterator::end ());

  if (! move ())
    *this = end ();

  return *this;
}

elfutils::v1::die_tree_iterator
elfutils::v1::die_tree_iterator::operator++ (int)
{
  die_tree_iterator ret = *this;
  ++*this;
  return ret;
}

Dwarf_Die &
elfutils::v1::die_tree_iterator::operator* ()
{
  assert (m_cuit != unit_iterator::end ());
  return m_die;
}

Dwarf_Die *
elfutils::v1::die_tree_iterator::operator-> ()
{
  return &**this;
}

elfutils::v1::die_tree_iterator
elfutils::v1::die_tree_iterator::parent ()
{
  assert (m_cuit != unit_iterator::end ());

  if (m_stack.empty ())
    return end ();

  die_tree_iterator ret = *this;
  ret.m_die = offdie (m_cuit, m_stack.back ());
  ret.m_stack.pop_back ();

  return ret;
}

std::vector <Dwarf_Die>
elfutils::v1::path_from_root (die_tree_iterator &it)
{
  std::vector <Dwarf_Die> ret;
  for (die_tree_iterator end = die_tree_iterator::end ();
       it != end; it = it.parent ())
    ret.push_back (*it);
  std::reverse (ret.begin (), ret.end ());
  return ret;
}
