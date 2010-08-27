/*
   Copyright (C) 2009,2010 Red Hat, Inc.
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

struct reporter
{
  checkstack const &stack;
  checkdescriptor const &cd;

  reporter (checkstack const &s, checkdescriptor const &a_cd);
  void operator () (char const *what, bool ext = false);
};

template <class T>
T *
dwarflint::check (checkstack &stack)
{
  void const *key = T::key ();
  T *c = static_cast <T *> (find_check (key));

  if (c == NULL)
    {
      checkdescriptor const &cd = T::descriptor ();

      struct popper {
	checkstack &guard_stack;
	popper (checkstack &a_guard_stack) : guard_stack (a_guard_stack) {}
	~popper () { guard_stack.pop_back (); }
      };

      // Put a marker there indicating that we are trying to satisfy
      // that dependency.
      bool inserted
	= _m_checks.insert (std::make_pair (key, (T *)marker)).second;
      assert (inserted || !"duplicate key");

      reporter report (stack, cd);
      try
	{
	  stack.push_back (&cd);
	  popper p (stack);

	  if (!_m_rules.should_check (stack))
	    throw check_base::unscheduled ();

	  // Now do the check.
	  c = new T (stack, *this);
	}
      catch (check_base::unscheduled &e)
	{
	  report ("skipped");
	  _m_checks.erase (key);
	  throw;
	}
      catch (...)
	{
	  // Nope, we failed.  Put the anchor there.
	  _m_checks[key] = NULL;
	  report ("FAIL");
	  throw;
	}

      report ("done");

      // On success, put the actual check object there instead of the
      // marker.
      _m_checks[key] = c;
    }
  return c;
}

template <class T>
inline T *
dwarflint::toplev_check (checkstack &stack,
			 __attribute__ ((unused)) T *tag)
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
  : public dwarflint::check_registrar::item
{
  reg ()
  {
    dwarflint::check_registrar::inst ()->add (this);
  }

  virtual void run (checkstack &stack, dwarflint &lint)
  {
    lint.toplev_check <T> (stack);
  }

  virtual void list () const
  {
    dwarflint::list_check (T::descriptor ());
  }
};

#endif//DWARFLINT_CHECKS_HH
