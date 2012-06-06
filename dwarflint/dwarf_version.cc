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

// The tables here capture attribute/allowed forms depending on DWARF
// version.  Apart from standardized DWARF formats, e.g. DWARF3+GNU is
// a version of its own.

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <map>
#include <cassert>
#include <string.h>

#include "../libdw/c++/dwarf"

#include "dwarf_version.hh"
#include "dwarf_2.hh"
#include "dwarf_3.hh"
#include "dwarf_4.hh"
#include "dwarf_gnu.hh"
#include "dwarf_mips.hh"
#include "check_debug_info.hh"

global_opt<void_option>
  opt_nognu ("Don't use GNU extension.", "nognu");

dw_class_set::dw_class_set (dw_class a, dw_class b, dw_class c,
			    dw_class d, dw_class e)
{
#define ADD(V) if (V != max_dw_class) (*this)[V] = true
  ADD (a);
  ADD (b);
  ADD (c);
  ADD (d);
  ADD (e);
#undef ADD
}

form::form (int a_name, dw_class_set a_classes,
	    form_width_t a_width, storage_class_t a_storclass,
	    form_bitness_t a_bitness)
  : _m_name (a_name)
  , _m_classes (a_classes)
  , _m_width (a_width)
  , _m_storclass (a_storclass)
  , _m_bitness (a_bitness)
{}

form::form (int a_name, dw_class_set a_classes,
	    form_width_special_t a_width, storage_class_t a_storclass,
	    form_bitness_t a_bitness)
  : _m_name (a_name)
  , _m_classes (a_classes)
  , _m_width (a_width)
  , _m_storclass (a_storclass)
  , _m_bitness (a_bitness)
{}

dw_class
dwarf_version::form_class (form const *form, attribute const *attribute) const
{
  assert (form != NULL);
  assert (attribute != NULL);
  dw_class_set result = form->classes ();
  result &= attribute->classes ();
  if (result.count () > 1)
    {
      dw_class ret = this->ambiguous_class (form, attribute, result);
      assert (ret < max_dw_class);
      assert (result[ret]);
      return ret;
    }
  else if (result.count () == 1)
    return static_cast<dw_class> (ffsl (result.to_ulong ()) - 1);
  else
    return max_dw_class;
}

form_width_t
form::width (cu_head const *cu_head) const
{
  switch (_m_width)
    {
    case fw_offset:
    case fw_address:
      if (unlikely (cu_head == NULL))
	return fw_unknown;
      if (_m_width == fw_offset)
	return static_cast<form_width_t> (cu_head->offset_size);
      else
	return static_cast<form_width_t> (cu_head->address_size);

    default:
      return static_cast<form_width_t> (_m_width);
    }
}

std::ostream &
operator << (std::ostream &os, form const &obj)
{
  return os << elfutils::dwarf::forms::identifier (obj.name ());
}

namespace
{
  dw_class_set
  include_indirect (dw_class_set a_classes)
  {
    a_classes.set (cl_indirect);
    return a_classes;
  }
}

attribute::attribute (int a_name, dw_class_set const &a_classes)
  : _m_name (a_name)
  , _m_classes (include_indirect (a_classes))
{}

std::ostream &
operator << (std::ostream &os, attribute const &obj)
{
  return os << elfutils::dwarf::attributes::identifier (obj.name ());
}


bool
dwarf_version::form_allowed (int form) const
{
  return get_form (form) != NULL;
}

bool
dwarf_version::form_allowed (attribute const *attr, form const *form) const
{
  dw_class_set const &attr_classes = attr->classes ();
  dw_class_set const &form_classes = form->classes ();
  return (attr_classes & form_classes).any ();
}

sibling_form_suitable_t
sibling_form_suitable (dwarf_version const *ver, form const *form)
{
  if (!ver->form_allowed (ver->get_attribute (DW_AT_sibling), form))
    return sfs_invalid;
  else if (form->name () == DW_FORM_ref_addr)
    return sfs_long;
  else
    return sfs_ok;
}

namespace
{
  class dwarf_version_union
    : public dwarf_version
  {
    dwarf_version const *_m_source;
    dwarf_version const *_m_extension;

  public:
    dwarf_version_union (dwarf_version const *source,
			 dwarf_version const *extension)
      : _m_source (source)
      , _m_extension (extension)
    {
    }

    template<class T>
    T const *
    lookfor (int name, T const*(dwarf_version::*getter) (int) const) const
    {
      if (T const *emt = (_m_extension->*getter) (name))
	return emt;
      else
	return (_m_source->*getter) (name);
    }

    form const *
    get_form (int form_name) const
    {
      return lookfor (form_name, &dwarf_version::get_form);
    }

    attribute const *
    get_attribute (int attribute_name) const
    {
      return lookfor (attribute_name, &dwarf_version::get_attribute);
    }

    dw_class
    ambiguous_class (form const *form,
		     attribute const *attribute,
		     dw_class_set const &candidates) const
    {
      dw_class ret = _m_extension->ambiguous_class (form, attribute, candidates);
      if (ret == max_dw_class)
	ret = _m_source->ambiguous_class (form, attribute, candidates);
      return ret;
    }

    bool
    form_allowed (attribute const *attr, form const *form) const
    {
      // In GNU mode any combination of new attribute/old form goes,
      // in strict mode only the latest.
      if (opt_nognu)
	return _m_extension->form_allowed (attr, form);
      else
	return (_m_source->form_allowed (attr, form)
		|| _m_extension->form_allowed (attr, form));
    }
  };
}

dwarf_version const *
dwarf_version::extend (dwarf_version const *source,
		       dwarf_version const *extension)
{
  assert (source != NULL);
  assert (extension != NULL);
  // this leaks, but we don't really care.  These objects have to live
  // the whole execution time anyway.
  return new dwarf_version_union (source, extension);
}

namespace
{
  dwarf_version const *get_ext ()
  {
    // xxx The GNU toolchain commonly uses DW_AT_MIPS_linkage_name,
    // which is part of the MIPS extensions.  So that's what we
    // return.  I wonder how to solve this "right".  We cannot simply
    // request DW_AT_producer/DW_AT_language values here, since we
    // need the version to know how to read these attributes in the
    // first place.

    if (opt_nognu)
      return dwarf_mips_ext ();
    else
      return dwarf_version::extend (dwarf_mips_ext (), dwarf_gnu_ext ());
  }
}

dwarf_version const *
dwarf_version::get (unsigned version)
{
  static dwarf_version const *ext = get_ext ();

  switch (version)
    {
    case 2:
      {
	static dwarf_version const *dw = extend (dwarf_2 (), ext);
	return dw;
      }

    case 3:
      {
	static dwarf_version const *dw = extend (dwarf_3 (), ext);
	return dw;
      }

    case 4:
      {
	static dwarf_version const *dw = extend (dwarf_4 (), ext);
	return dw;
      }

    default:
      return NULL;
    };
}

dwarf_version const *
dwarf_version::get_latest ()
{
  return get (4);
}
