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
#include <bitset>
#include "check_debug_info.ii"

typedef int die_tag;
class locexpr_op {};

enum dw_class
  {
    cl_address,
    cl_block,
    cl_constant,
    cl_exprloc,
    cl_flag,
    cl_reference,
    cl_string,
    cl_loclistptr,
    cl_lineptr,
    cl_macptr,
    cl_rangelistptr,
    max_dw_class
  };

class dw_class_set
  : public std::bitset<max_dw_class>
{
public:
  dw_class_set (dw_class a = max_dw_class, dw_class b = max_dw_class,
		dw_class c = max_dw_class, dw_class d = max_dw_class,
		dw_class e = max_dw_class);
};

enum form_width_t
  {
    fw_0 = 0,
    fw_1 = 1,
    fw_2 = 2,
    fw_4 = 4,
    fw_8 = 8,
    fw_leb,
    fw_unknown
  };

enum storage_class_t
  {
    sc_value,
    sc_block,
    sc_string,
  };

class form
{
public:
  virtual int name () const = 0;

  /// Answer set of DWARF classes that this form can have.
  virtual dw_class_set const &classes () const = 0;

  /// Return width of data stored with given form.  CU may be NULL if
  /// you are sure that the form size doesn't depend on addr_64 or
  /// off.  Forms for which width makes no sense, such as
  /// DW_FORM_string, get fw_unknown.  Unknown forms get an assert.
  virtual form_width_t width (cu const *cu = NULL) const = 0;

  //virtual storage_class_t storage_class () const = 0;

  virtual ~form () {}
};

class attribute
{
public:
  virtual int name () const = 0;
  virtual dw_class_set const &classes () const = 0;
  virtual ~attribute () {}
};

class dwarf_version
{
public:
  /// Return form object for given form name.  Return NULL for unknown
  /// forms.
  virtual form const *get_form (int form_name) const = 0;

  /// Shortcut for get_form (form_name) != NULL.
  bool form_allowed (int form_name) const;

  /// Figure out whether, in given DWARF version, given attribute is
  /// allowed to have given form.
  virtual bool form_allowed (int attr_name, int form_name) const = 0;

  /// Return dwarf_version object for given DWARF version.
  static dwarf_version const *get (unsigned version)
    __attribute__ ((pure));

  /// Return dwarf_version object for latest supported DWARF version.
  static dwarf_version const *get_latest ()
    __attribute__ ((pure));
};

/// Check that the form is suitable for the DW_AT_sibling attribute.
enum sibling_form_suitable_t
  {
    sfs_ok,      ///< This form is OK for DW_AT_sibling
    sfs_long,    ///< Global reference form, unnecessary for DW_AT_sibling
    sfs_invalid, ///< This form isn't allowed at DW_AT_sibling
  };
sibling_form_suitable_t sibling_form_suitable (dwarf_version const *ver,
					       int form);

#endif//DWARFLINT_TABLES_HH
