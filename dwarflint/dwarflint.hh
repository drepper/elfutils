/* Pedantic checking of DWARF files
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

#ifndef DWARFLINT_HH
#define DWARFLINT_HH

#include <map>
#include <vector>
#include <stdexcept>
#include <string>
#include <iosfwd>

#include "../libelf/libelf.h"
#include "checks.ii"
#include "checkdescriptor.ii"

class checkstack
  : public std::vector <checkdescriptor const *>
{};
std::ostream &operator << (std::ostream &o, checkstack const &stack);

struct check_rule
{
  enum action_t
    {
      forbid,
      request,
    };

  std::string name;
  action_t action;

  check_rule (std::string const &a_name, action_t an_action)
    : name (a_name)
    , action (an_action)
  {}
};
class check_rules
  : public std::vector<check_rule>
{
  friend class dwarflint;
  bool should_check (checkstack const &stack) const;
};

class dwarflint
{
  typedef std::map <void const *, class check_base *> check_map;
  check_map _m_checks;
  char const *_m_fname;
  int _m_fd;
  check_rules const &_m_rules;

  static void *const marker;

  // Return a pointer to check, or NULL if the check hasn't been done
  // yet.  Throws check_base::failed if the check was requested
  // earlier but failed, or aborts program via assertion if recursion
  // was detected.
  void *find_check (void const *key);

public:
  struct check_registrar
  {
    struct item
    {
      virtual void run (checkstack &stack, dwarflint &lint) = 0;
      virtual checkdescriptor descriptor () const = 0;
    };

    static check_registrar *inst ()
    {
      static check_registrar inst;
      return &inst;
    }

    void add (item *i)
    {
      _m_items.push_back (i);
    }

    void list_checks () const;

  private:
    friend class dwarflint;
    void enroll (dwarflint &lint);

    std::vector <item *> _m_items;
  };

  dwarflint (char const *fname, check_rules const &rules);
  ~dwarflint ();
  int fd () { return _m_fd; }
  char const *fname () { return _m_fname; }

  template <class T> T *check (checkstack &stack);

  template <class T>
  T *
  check (checkstack &stack,
	 __attribute__ ((unused)) T *fake)
  {
    return check<T> (stack);
  }

  template <class T>
  T *toplev_check (checkstack &stack, T *tag = NULL);
};

#endif//DWARFLINT_HH
