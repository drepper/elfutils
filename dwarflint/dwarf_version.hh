/* Dwarf version tables.

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

#ifndef DWARFLINT_DWARF_VERSION_HH
#define DWARFLINT_DWARF_VERSION_HH

#include <bitset>
#include <iosfwd>
#include "check_debug_info_i.hh"
#include "dwarf_version_i.hh"
#include "option.hh"

extern global_opt<void_option> opt_nognu;

enum dw_class
  {
    cl_indirect,
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
    fw_sleb,
    fw_uleb,
    fw_unknown,
  };

/// Special forms for use in DWARF tables.  These never leak out to
/// the user of dwarf_version.
enum form_width_special_t
  {
    fw_offset = fw_unknown + 1,
    fw_address,
  };

enum storage_class_t
  {
    sc_value,
    sc_block,
    sc_string,
  };

enum form_bitness_t
  {
    fb_any, ///< Form is allowed in all CUs
    fb_32,  ///< Form is allowed only in 32-bit CUs
    fb_64,  ///< Form is allowed only in 64-bit CUs
  };

class form
{
  int const _m_name;
  dw_class_set const _m_classes;
  int const _m_width;
  storage_class_t const _m_storclass;
  form_bitness_t _m_bitness;

public:
  form (int name, dw_class_set classes,
	form_width_t width, storage_class_t storclass,
	form_bitness_t bitness = fb_any);

  form (int name, dw_class_set classes,
	form_width_special_t width, storage_class_t storclass,
	form_bitness_t bitness = fb_any);

  int
  name () const
  {
    return _m_name;
  }

  /// Answer set of DWARF classes that this form can have.
  dw_class_set const &
  classes () const
  {
    return _m_classes;
  }

  /// Return width of data stored with given form.  CU may be NULL if
  /// you are sure that the form size doesn't depend on bitness of
  /// address_size or offset_size.
  ///
  /// Forms for which width makes no sense (namely those in the
  /// storage class of sc_string) get fw_unknown.  Unknown forms get
  /// an assert.
  ///
  /// Return value is never fw_offset or fw_address.  These get
  /// resolved to fw_4 or fw_8 depending on corresponding value in
  /// CU_HEAD.
  form_width_t width (cu_head const *cu_head) const;

  /// Return storage class of given form.  Closely related to width.
  storage_class_t
  storage_class () const
  {
    return _m_storclass;
  }

  form_bitness_t
  bitness () const
  {
    return _m_bitness;
  }
};
std::ostream &operator << (std::ostream &os, form const &obj);


class attribute
{
  int const _m_name;
  dw_class_set const _m_classes;

public:
  /// NB this ctor automatically adds cl_indirect to a_classes.
  attribute (int a_name, dw_class_set const &a_classes);

  int
  name () const
  {
    return _m_name;
  }

  /// Answer set of DWARF classes that this form can have.
  dw_class_set const &
  classes () const
  {
    return _m_classes;
  }
};
std::ostream &operator << (std::ostream &os, attribute const &obj);

class dwarf_version
{
public:
  /// Return form object for given form name.  Return NULL for unknown
  /// forms.
  virtual form const *get_form (int form_name) const = 0;

  /// Return attribute object for given attribute name.  Return NULL
  /// for unknown attributes;
  virtual attribute const *get_attribute (int attribute_name) const = 0;

  /// If more than one class ends up as a candidate after the request
  /// to form_class, this function is called to resolve the ambiguity.
  virtual dw_class
  ambiguous_class (__attribute__ ((unused)) form const *form,
		   __attribute__ ((unused)) attribute const *attribute,
		   __attribute__ ((unused)) dw_class_set const &candidates)
    const
  {
    return max_dw_class; // = we don't know.  This will assert back in caller.
  }

  /// Shortcut for get_form (form_name) != NULL.
  bool form_allowed (int form_name) const;

  /// Figure out whether, in given DWARF version, given attribute is
  /// allowed to have given form.
  virtual bool form_allowed (attribute const *attr, form const *form) const
    __attribute__ ((nonnull (1, 2)));

  /// Answer a class of FORM given ATTRIBUTE as a context.  If there's
  /// exactly one candidate class, that's the one answered.  If
  /// there's more, ambiguous_class is called to resolve the
  /// ambiguity.  If there's no candidate, then the request is
  /// invalid, you must validate the form via form_allowed before
  /// calling this.
  dw_class form_class (form const *form, attribute const *attribute) const;


  /// Return dwarf_version object for given DWARF version.
  static dwarf_version const *get (unsigned version)
    __attribute__ ((pure));

  /// Return dwarf_version object for latest supported DWARF version.
  static dwarf_version const *get_latest ()
    __attribute__ ((pure));

  /// Return dwarf_version that represents SOURCE extended with
  /// EXTENSION.  Currently this probably has no use, but one obvious
  /// candidate usage is representing GNU extensions over core DWARF.
  /// Extension can contain overrides of the source dwarf_version
  /// object, and these overrides take precedence.
  static dwarf_version const *extend (dwarf_version const *source,
				      dwarf_version const *extension);
};

/// Check that the form is suitable for the DW_AT_sibling attribute.
enum sibling_form_suitable_t
  {
    sfs_ok,      ///< This form is OK for DW_AT_sibling
    sfs_long,    ///< Global reference form, unnecessary for DW_AT_sibling
    sfs_invalid, ///< This form isn't allowed at DW_AT_sibling
  };
sibling_form_suitable_t sibling_form_suitable (dwarf_version const *ver,
					       form const *form)
  __attribute__ ((nonnull (1, 2)));

#endif//DWARFLINT_DWARF_VERSION_HH
