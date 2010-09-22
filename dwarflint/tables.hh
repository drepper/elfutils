/* Dwarf version tables.

   Copyright (C) 2009, 2010 Red Hat, Inc.
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

#ifndef DWARFLINT_TABLES_HH
#define DWARFLINT_TABLES_HH

#include <set>

typedef int form;
typedef int attr;
typedef int die_tag;
class locexpr_op {};

class dwarf_version
{
protected:
  typedef std::set <form> form_set_t;

private:
  inline static bool find_form (form_set_t const &s, int f)
  {
    return s.find (f) != s.end ();
  }

public:
  // Answer all known forms.
  virtual form_set_t const &allowed_forms () const = 0;

  // Answer all forms allowed in theory for this attribute.
  virtual form_set_t const &allowed_forms (attr at) const = 0;

  // Answer forms allowed for this attribute at DIE with that tag.
  virtual form_set_t const &allowed_forms (attr at, die_tag tag) const = 0;

public:
  bool form_allowed (form f) const
  {
    return find_form (allowed_forms (), f);
  }

  bool form_allowed (attr at, form f) const
  {
    return find_form (allowed_forms (at), f);
  }

  bool form_allowed (attr at, form f, die_tag tag) const
  {
    return find_form (allowed_forms (at, tag), f);
  }

  int check_sibling_form (int form) const;

  static dwarf_version const *get (unsigned version)
    __attribute__ ((pure));

  static dwarf_version const *get_latest ()
    __attribute__ ((pure));
};

#endif//DWARFLINT_TABLES_HH
