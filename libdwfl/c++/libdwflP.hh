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

#ifndef _CPP_LIBDWFLP_H
#define _CPP_LIBDWFLP_H 1

#include <stdexcept>
#include <cassert>

static inline void
throw_libdwfl (int dwflerr = 0)
{
  if (dwflerr == 0)
    dwflerr = dwfl_errno ();
  assert (dwflerr != 0);
  throw std::runtime_error (dwfl_errmsg (dwflerr));
}

#endif
