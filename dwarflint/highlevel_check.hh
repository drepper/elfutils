/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECKS_HIGH_HH
#define DWARFLINT_CHECKS_HIGH_HH

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "checks.hh"
#include "../libdw/c++/dwarf"
#include "../libdwfl/libdwfl.h"

class open_highlevel_dwarf
  : public check<open_highlevel_dwarf>
{
  Dwfl *const _m_dwfl;
public:
  static checkdescriptor const *descriptor () {
    static checkdescriptor cd
      (checkdescriptor::create ("open_highlevel_dwarf")
       .hidden ());
    return &cd;
  }

  Dwarf *const c_dw;
  elfutils::dwarf const dw;
  open_highlevel_dwarf (checkstack &stack, dwarflint &lint);
  ~open_highlevel_dwarf ();
};

struct highlevel_check_i
{
  open_highlevel_dwarf *_m_loader;
  Dwarf *const c_dw;
  elfutils::dwarf const &dw;

  highlevel_check_i (checkstack &stack, dwarflint &lint)
    : _m_loader (lint.check (stack, _m_loader))
    , c_dw (_m_loader->c_dw)
    , dw (_m_loader->dw)
  {}
};

template<class T>
class highlevel_check
  : public check<highlevel_check<T> >
  , public highlevel_check_i
{
  open_highlevel_dwarf *_m_loader;
public:
  static checkdescriptor const *descriptor () {
    static checkdescriptor cd ("highlevel_check");
    return &cd;
  }

  highlevel_check (checkstack &stack, dwarflint &lint)
    : highlevel_check_i (stack, lint)
  {}
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

#endif//DWARFLINT_CHECKS_HIGH_HH
