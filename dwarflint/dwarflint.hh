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

#ifndef DWARFLINT_HH
#define DWARFLINT_HH

#include <map>
#include <vector>
#include <stdexcept>
#include <iosfwd>

#include "../libelf/libelf.h"
#include "checks_i.hh"
#include "checkdescriptor_i.hh"
#include "checkrule.hh"
#include "check_registrar.hh"
#include "dwarflint_i.hh"
#include "highlevel_check_i.hh"

// Classes for full-blown check passes.
struct main_check_item
{
  virtual checkdescriptor const *descriptor () const = 0;
  virtual ~main_check_item () {}
  virtual void run (checkstack &stack, dwarflint &lint) = 0;
};

class main_check_registrar
  : public check_registrar_T<main_check_item>
{
public:
  friend class dwarflint;
  void run (dwarflint &lint);
};

class checkstack
  : public std::vector <checkdescriptor const *>
{};
std::ostream &operator << (std::ostream &o, checkstack const &stack);


class dwarflint
{
  typedef std::map <void const *, class check_base *> check_map;
  check_map _m_checks;
  char const *_m_fname;
  int _m_fd;
  checkrules const &_m_rules;

  static void *const marker;

  // Return a pointer to check, or NULL if the check hasn't been done
  // yet.  Throws check_base::failed if the check was requested
  // earlier but failed, or aborts program via assertion if recursion
  // was detected.
  void *find_check (void const *key);

  template <class T>
  static check_base *
  create_check_object (checkstack &stack, dwarflint &lint)
  {
    return new T (stack, lint);
  }

  void *dispatch_check (checkstack &stack,
			checkdescriptor const &cd,
			void const *key,
			check_base *(* create) (checkstack &, dwarflint &));

public:
  dwarflint (char const *fname, checkrules const &rules);
  ~dwarflint ();
  int fd () { return _m_fd; }
  char const *fname () { return _m_fname; }

  template <class T>
  T *
  check (checkstack &stack)
  {
    void const *key = T::key ();
    T *c = static_cast <T *> (find_check (key));
    checkdescriptor const &cd = *T::descriptor ();

    if (c == NULL)
      c = (T *)dispatch_check (stack, cd, key, &create_check_object<T>);

    return c;
  }

  template <class T>
  T *
  check (checkstack &stack, T *)
  {
    return check<T> (stack);
  }

  template <class T>
  T *toplev_check (checkstack &stack, T *fake = NULL);

  template <class T>
  T *
  check_if (bool whether, checkstack &stack,
	    __attribute__ ((unused)) T *fake = NULL)
  {
    if (whether)
      return check<T> (stack);
    else
      return NULL;
  }

  checkrules const &
  rules () const
  {
    return _m_rules;
  }

  static main_check_registrar *main_registrar ();

  static void list_checks ();
};

#endif//DWARFLINT_HH
