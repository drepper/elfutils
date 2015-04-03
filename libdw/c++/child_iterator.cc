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

#include "libdw"
#include "libdwP.hh"

#include <cstring>

attribute_hidden
elfutils::v1::child_iterator::child_iterator (end_it)
{
  m_die.addr = NULL;
}

elfutils::v1::child_iterator::child_iterator (Dwarf_Die parent)
{
  if (! dwpp_child (parent, m_die))
    *this = end ();
}

elfutils::v1::child_iterator
elfutils::v1::child_iterator::end ()
{
  return child_iterator (end_it ());
}

bool
elfutils::v1::child_iterator::operator== (child_iterator const &that) const
{
  return m_die.addr == that.m_die.addr;
}

bool
elfutils::v1::child_iterator::operator!= (child_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::v1::child_iterator &
elfutils::v1::child_iterator::operator++ ()
{
  assert (m_die.addr != NULL);

  if (! dwpp_siblingof (m_die, m_die))
    *this = end ();

  return *this;
}

elfutils::v1::child_iterator
elfutils::v1::child_iterator::operator++ (int)
{
  child_iterator ret = *this;
  ++*this;
  return ret;
}

Dwarf_Die &
elfutils::v1::child_iterator::operator* ()
{
  assert (m_die.addr != NULL);
  return m_die;
}

Dwarf_Die *
elfutils::v1::child_iterator::operator-> ()
{
  return &**this;
}
