/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECKS_HIGH_HH
#define DWARFLINT_CHECKS_HIGH_HH

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "checks.hh"
#include "check_debug_info.hh"

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

#endif//DWARFLINT_CHECKS_HIGH_HH
