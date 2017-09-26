/*
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

#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include "locus.hh"
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
