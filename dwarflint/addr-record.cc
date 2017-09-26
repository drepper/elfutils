/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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
