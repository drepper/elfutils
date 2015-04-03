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

#include "libdw"
#include "libdwP.hh"

attribute_hidden
bool
elfutils::v1::unit_iterator::move ()
{
  m_old_offset = m_offset;
  size_t hsize;
  int rc = dwarf_next_unit (m_dw, m_offset, &m_offset, &hsize,
			    &m_info.version, &m_info.abbrev_offset,
			    &m_info.address_size, &m_info.offset_size,
			    m_types ? &m_info.type_signature : NULL, NULL);
  if (rc < 0)
    throw_libdw ();

  if (rc != 0 && m_types)
    return false;
  else if (rc != 0)
    {
      m_types = true;
      m_offset = 0;
      m_old_offset = 0;
      return move ();
    }
  else
    {
      m_info.cudie = (m_types ? dwpp_offdie_types : dwpp_offdie)
	(m_dw, m_old_offset + hsize);
      return true;
    }
}

attribute_hidden
elfutils::v1::unit_iterator::unit_iterator (end_it)
  : m_dw (NULL)
{}

elfutils::v1::unit_iterator::unit_iterator (Dwarf *dw)
  : m_dw (dw)
  , m_offset (0)
  , m_old_offset (0)
  , m_types (false)
{
  // Initial move which may turn this into an end iterator.
  ++*this;
}

elfutils::v1::unit_iterator::unit_iterator (Dwarf *dw, Dwarf_Die cudie)
  : m_dw (dw)
  , m_offset (dwarf_dieoffset (&cudie) - dwarf_cuoffset (&cudie))
  , m_old_offset (0)
  , m_types (dwarf_tag (&cudie) == DW_TAG_type_unit)
{
  // Initial move, which shouldn't change this into an end iterator,
  // we were given a valid CU DIE!
  bool alive = move ();
  assert (alive);
}

elfutils::v1::unit_iterator
elfutils::v1::unit_iterator::end ()
{
  return unit_iterator (end_it ());
}

bool
elfutils::v1::unit_iterator::operator== (unit_iterator const &that) const
{
  return (m_dw == NULL && that.m_dw == NULL)
    || (m_dw != NULL && that.m_dw != NULL
	&& m_types == that.m_types
	&& m_offset == that.m_offset);
}

bool
elfutils::v1::unit_iterator::operator!= (unit_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::v1::unit_iterator &
elfutils::v1::unit_iterator::operator++ ()
{
  assert (m_dw != NULL);

  if (! move ())
    *this = end ();

  return *this;
}

elfutils::v1::unit_iterator
elfutils::v1::unit_iterator::operator++ (int)
{
  unit_iterator tmp = *this;
  ++*this;
  return tmp;
}

elfutils::v1::unit_info &
elfutils::v1::unit_iterator::operator* ()
{
  assert (m_dw != NULL);
  return m_info;
}

elfutils::v1::unit_info *
elfutils::v1::unit_iterator::operator-> ()
{
  return &**this;
}
