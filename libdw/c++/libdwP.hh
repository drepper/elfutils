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

#ifndef _CPP_LIBDWP_H
#define _CPP_LIBDWP_H 1

#include <stdexcept>
#include <cassert>
#include <cstdlib>

inline void
throw_libdw (int dwerr = 0)
{
  if (dwerr == 0)
    dwerr = dwarf_errno ();
  assert (dwerr != 0);
  throw std::runtime_error (dwarf_errmsg (dwerr));
}

inline bool
dwpp_child (Dwarf_Die &die, Dwarf_Die &result)
{
  int ret = dwarf_child (&die, &result);
  if (ret < 0)
    throw_libdw ();
  return ret == 0;
}

inline bool
dwpp_siblingof (Dwarf_Die &die, Dwarf_Die &result)
{
  switch (dwarf_siblingof (&die, &result))
    {
    case -1:
      throw_libdw ();
    case 0:
      return true;
    case 1:
      return false;
    default:
      std::abort ();
    }
}

inline Dwarf_Die
dwpp_offdie (Dwarf *dbg, Dwarf_Off offset)
{
  Dwarf_Die result;
  if (dwarf_offdie (dbg, offset, &result) == NULL)
    throw_libdw ();
  return result;
}

inline Dwarf_Die
dwpp_offdie_types (Dwarf *dbg, Dwarf_Off offset)
{
  Dwarf_Die result;
  if (dwarf_offdie_types (dbg, offset, &result) == NULL)
    throw_libdw ();
  return result;
}

#endif
