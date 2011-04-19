/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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

#include "addr-record.hh"

size_t
addr_record::find (uint64_t addr) const
{
  size_t a = 0;
  size_t b = size ();

  while (a < b)
    {
      size_t i = (a + b) / 2;
      uint64_t v = (*this)[i];

      if (v > addr)
	b = i;
      else if (v < addr)
	a = i + 1;
      else
	return i;
    }

  return a;
}

bool
addr_record::has_addr (uint64_t addr) const
{
  if (begin () == end ()
      || addr < front ()
      || addr > back ())
    return false;

  const_iterator it = begin () + find (addr);
  return it != end () && *it == addr;
}

void
addr_record::add (uint64_t addr)
{
  iterator it = begin () + find (addr);
  if (it == end () || *it != addr)
    insert (it, addr);
}
