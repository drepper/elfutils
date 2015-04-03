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

namespace
{
  struct cb_data
  {
    // The visited attribute.
    Dwarf_Attribute *at;

    // Whether this is second pass through the callback.
    bool been;
  };

  int
  callback (Dwarf_Attribute *at, void *data)
  {
    cb_data *d = static_cast <cb_data *> (data);
    if (d->been)
      return DWARF_CB_ABORT;

    *d->at = *at;
    d->been = true;

    // Do a second iteration to find the next offset.
    return DWARF_CB_OK;
  }
}

attribute_hidden
bool
elfutils::v1::attr_iterator::move ()
{
  cb_data data = {&m_at, false};

  // m_offset of 1 means there are no more attributes.
  if (m_offset != 1)
    {
      m_offset = dwarf_getattrs (m_die, &callback, &data, m_offset);
      if (m_offset == -1)
	throw_libdw ();
    }

  return data.been;
}

attribute_hidden
elfutils::v1::attr_iterator::attr_iterator (end_it)
  : m_die (NULL)
{}

elfutils::v1::attr_iterator::attr_iterator (Dwarf_Die *die)
  : m_die (die)
  , m_at ((Dwarf_Attribute) {0, 0, NULL, NULL})
  , m_offset (0)
{
  // Initial move, which can turn this into an end iterator.
  ++*this;
}

elfutils::v1::attr_iterator
elfutils::v1::attr_iterator::end ()
{
  return attr_iterator (end_it ());
}

bool
elfutils::v1::attr_iterator::operator== (attr_iterator const &that) const
{
  return (m_die == NULL && that.m_die == NULL)
    || (m_die != NULL && that.m_die != NULL
	&& m_offset == that.m_offset
	&& m_at.code == that.m_at.code);
}

bool
elfutils::v1::attr_iterator::operator!= (attr_iterator const &that) const
{
  return ! (*this == that);
}

elfutils::v1::attr_iterator &
elfutils::v1::attr_iterator::operator++ ()
{
  assert (m_die != NULL);

  if (! move ())
    *this = end ();

  return *this;
}

elfutils::v1::attr_iterator
elfutils::v1::attr_iterator::operator++ (int)
{
  attr_iterator tmp = *this;
  ++*this;
  return tmp;
}

Dwarf_Attribute &
elfutils::v1::attr_iterator::operator* ()
{
  assert (m_die != NULL);
  return m_at;
}

Dwarf_Attribute *
elfutils::v1::attr_iterator::operator-> ()
{
  return &**this;
}
