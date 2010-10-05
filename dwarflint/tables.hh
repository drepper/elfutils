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
#include "check_debug_info.ii"

typedef int form; // xxx get rid of this or something, it collides
		  // with the x_form stuff.
typedef int attr;
typedef int die_tag;
class locexpr_op {};

class x_form; //  xxx and rename this guy

class dwarf_version
{
public:
  enum form_width_t
    {
      fw_0 = 0,
      fw_1 = 1,
      fw_2 = 2,
      fw_4 = 4,
      fw_8 = 8,
      fw_uleb,
      fw_unknown
    };
  // Return width of data stored with given form.  CU may be NULL if
  // you are sure that the form size doesn't depend on addr_64 or off.
  // Forms for which width makes no sense, such as DW_FORM_string, get
  // fw_unknown.  Unknown forms get an assert.
  virtual form_width_t
  form_width (int form, struct cu const *cu = NULL) const = 0;

public:
  virtual bool form_allowed (form f) const = 0;

  virtual x_form const *get_form (int name) const = 0;

  virtual bool form_allowed (attr at, form f) const = 0;

  int check_sibling_form (int form) const;

  static dwarf_version const *get (unsigned version)
    __attribute__ ((pure));

  static dwarf_version const *get_latest ()
    __attribute__ ((pure));
};

#endif//DWARFLINT_TABLES_HH
