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

// The tables here capture attribute/allowed forms depending on DWARF
// version.  Apart from standardized DWARF formats, e.g. DWARF3+GNU is
// a version of its own.

#include "dwarf_version.hh"
#include "dwarf_2.hh"
#include "dwarf_3.hh"
#include "dwarf_4.hh"

#include "../libdw/dwarf.h"
#include <map>
#include <cassert>

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

bool
dwarf_version::form_allowed (int form) const
{
  return get_form (form) != NULL;
}

bool
dwarf_version::form_allowed (int attribute_name, int form_name) const
{
  attribute const *attribute = this->get_attribute (attribute_name);
  assert (attribute != NULL);
  dw_class_set const &attr_classes = attribute->classes ();

  form const *form = this->get_form (form_name);
  assert (form != NULL);
  dw_class_set const &form_classes = form->classes ();

  return (attr_classes & form_classes).any ();
}

sibling_form_suitable_t
sibling_form_suitable (dwarf_version const *ver, int form)
{
  if (!ver->form_allowed (DW_AT_sibling, form))
    return sfs_invalid;
  else if (form == DW_FORM_ref_addr)
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

dwarf_version const *
dwarf_version::get (unsigned version)
{
  switch (version)
    {
    case 2: return dwarf_2 ();
    case 3: return dwarf_3 ();
    case 4: return dwarf_4 ();
    default: return NULL;
    };
}

dwarf_version const *
dwarf_version::get_latest ()
{
  return get (4);
}
