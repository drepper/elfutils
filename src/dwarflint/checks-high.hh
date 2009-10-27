/* Pedantic checking of DWARF files.
   Copyright (C) 2009 Red Hat, Inc.
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

#include "checks-low.hh"
#include "config.h"
#include "c++/dwarf"

template<class T>
class highlevel_check
  : public check<highlevel_check<T> >
{
  ::Dwarf *_m_handle;

public:
  elfutils::dwarf dw;

  // xxx this will throw an exception on <c++/dwarf> or <libdw.h>
  // failure.  We need to catch it and convert to check_base::failed.
  explicit highlevel_check (dwarflint &lint)
    : _m_handle (dwarf_begin_elf (lint.elf (), DWARF_C_READ, NULL))
    , dw (_m_handle)
  {
    if (!do_high_level)
      throw check_base::unscheduled ();
  }

  ~highlevel_check ()
  {
    dwarf_end (_m_handle);
  }
};

template <class T>
inline where
to_where (T const &die)
{
  where ret = WHERE (sec_info, NULL);
  where_reset_1 (&ret, 0);
  where_reset_2 (&ret, die.offset ());
  return ret;
}
