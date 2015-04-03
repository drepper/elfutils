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

#include "libdwfl"
#include "libdwflP.hh"

namespace
{
  struct cb_data
  {
    Dwfl_Module **m_modulep;
  };

  int
  callback (Dwfl_Module *mod, void **, const char *,
	    Dwarf_Addr, void *arg)
  {
    cb_data *d = static_cast <cb_data *> (arg);
    *d->m_modulep = mod;
    return DWARF_CB_ABORT;
  }
}

attribute_hidden
bool
elfutils::v1::dwfl_module_iterator::move ()
{
  cb_data d = {&m_module};
  m_offset = dwfl_getmodules (m_dwfl, callback, &d, m_offset);
  if (m_offset == -1)
    throw_libdwfl ();
  return m_offset != 0;
}

attribute_hidden
elfutils::v1::dwfl_module_iterator::dwfl_module_iterator (end_it)
  : m_dwfl (NULL)
{}

elfutils::v1::dwfl_module_iterator::dwfl_module_iterator (Dwfl *dwfl)
  : m_dwfl (dwfl)
  , m_offset (0)
{
  // Initial move, which can turn this into an end iterator.
  ++*this;
}

elfutils::v1::dwfl_module_iterator
elfutils::v1::dwfl_module_iterator::end ()
{
  return dwfl_module_iterator (end_it ());
}

elfutils::v1::dwfl_module_iterator &
elfutils::v1::dwfl_module_iterator::operator++ ()
{
  assert (m_dwfl != NULL);

  if (! move ())
    *this = end ();

  return *this;
}

elfutils::v1::dwfl_module_iterator
elfutils::v1::dwfl_module_iterator::operator++ (int)
{
  dwfl_module_iterator ret = *this;
  ++*this;
  return ret;
}

bool
elfutils::v1::dwfl_module_iterator::operator== (dwfl_module_iterator
							const &that) const
{
  return m_dwfl == that.m_dwfl
    && (m_dwfl == NULL || m_offset == that.m_offset);
}

bool
elfutils::v1::dwfl_module_iterator::operator!= (dwfl_module_iterator
							const &that) const
{
  return ! (*this == that);
}

Dwfl_Module &
elfutils::v1::dwfl_module_iterator::operator* () const
{
  assert (m_dwfl != NULL);
  return *m_module;
}

Dwfl_Module *
elfutils::v1::dwfl_module_iterator::operator-> () const
{
  return &**this;
}
