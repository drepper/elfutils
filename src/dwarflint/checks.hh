/*
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

#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include <string>
#include "where.h"
#include "dwarflint.hh"

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
  friend X *dwarflint::check ();
  static void const *key ()
  {
    return reinterpret_cast <void const *> (&key);
  }
};

template <class T>
T *
dwarflint::check ()
{
  void const *key = T::key ();
  check_map::iterator it = _m_checks.find (key);

  T *c;
  if (it != _m_checks.end ())
    {
      c = static_cast <T *> (it->second);

      // We already tried to do the check, but failed.
      if (c == NULL)
	throw check_base::failed ();
    }
  else
    {
      // Put a marker there saying that we tried to do the check, but
      // it failed.
      if (!_m_checks.insert (std::make_pair (key, (T *)0)).second)
	throw std::runtime_error ("duplicate key");

      // Now do the check.
      c = new T (*this);

      // On success, put the actual check object there instead of the
      // marker.
      _m_checks[key] = c;
    }
  return c;
}

template <class T>
inline T *
dwarflint::toplev_check (__attribute__ ((unused)) T *tag)
{
  try
    {
      return check<T> ();
    }
  catch (check_base::failed const &f)
    {
      return NULL;
    }
}

template <class T>
struct reg
  : public dwarflint::check_registrar::item
{
  reg ()
  {
    dwarflint::check_registrar::inst ()->add (this);
  }

  virtual void run (dwarflint &lint)
  {
    lint.toplev_check <T> ();
  }
};

#endif//DWARFLINT_CHECKS_HH
