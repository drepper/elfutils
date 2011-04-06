/*
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

#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include "where.h"
#include "dwarflint.hh"
#include "checkdescriptor.hh"
#include "messages.hh"
#include "check_registrar.hh"

#include <string>
#include <cassert>

struct check_base
{
  struct failed {};
  struct unscheduled: public failed {};
  virtual ~check_base () {}
};

template<class T>
class check
  : public check_base
{
private:
  template <class X>
  friend X *dwarflint::check (checkstack &stack);
  static void const *key ()
  {
    return reinterpret_cast <void const *> (&key);
  }
};

class popper
{
  checkstack &_m_guard_stack;

public:
  popper (checkstack &guard_stack)
    : _m_guard_stack (guard_stack)
  {}

  ~popper ()
  {
    _m_guard_stack.pop_back ();
  }
};

template <class T>
inline T *
dwarflint::toplev_check (checkstack &stack, T *)
{
  try
    {
      return check<T> (stack);
    }
  catch (check_base::failed const &f)
    {
      return NULL;
    }
}

template <class T>
struct reg
  : public main_check_item
{
  reg ()
  {
    dwarflint::main_registrar ()->push_back (this);
  }

  virtual void run (checkstack &stack, dwarflint &lint)
  {
    lint.toplev_check <T> (stack);
  }

  virtual checkdescriptor const *descriptor () const
  {
    return T::descriptor ();
  }
};

#endif//DWARFLINT_CHECKS_HH
