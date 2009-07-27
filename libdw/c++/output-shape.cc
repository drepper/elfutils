/* elfutils::dwarf_output abbrev generation.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include <config.h>
#include "dwarf_output"
#include "../../src/dwarfstrings.h"

using namespace elfutils;

static inline int
attr_form (int tag, const dwarf_output::attribute &attr)
{
  switch (attr.second.what_space ())
    {
    case dwarf::VS_address:
      return DW_FORM_addr;

    case dwarf::VS_flag:
      return DW_FORM_flag;

    case dwarf::VS_reference:
      return DW_FORM_ref_addr;

    case dwarf::VS_string:
    case dwarf::VS_identifier:
      return DW_FORM_string;

    case dwarf::VS_constant:
      if (! attr.second.constant_is_integer ())
	return DW_FORM_block;
      /* Fall through.  */

    case dwarf::VS_dwarf_constant:
    case dwarf::VS_source_line:
    case dwarf::VS_source_column:
      return DW_FORM_udata;

    case dwarf::VS_location:
      if (!attr.second.location ().is_list ())
	return DW_FORM_block;
      /* Fall through.  */

    case dwarf::VS_lineptr:
    case dwarf::VS_macptr:
    case dwarf::VS_rangelistptr:
      /* For class *ptr (including loclistptr), the one of data[48] that
	 matches offset_size is the only form encoding to use.  Other data*
	 forms can mean the attribute is class constant instead.  */
      return DW_FORM_data4;

    case dwarf::VS_source_file:
      switch (attr.first)
	{
	case DW_AT_decl_file:
	case DW_AT_call_file:
	  return DW_FORM_udata;

	case DW_AT_comp_dir:
	  return DW_FORM_string;

	case DW_AT_name:
	  switch (tag)
	    {
	    case DW_TAG_compile_unit:
	    case DW_TAG_partial_unit:
	      return DW_FORM_string;
	    }
	  break;
	}
      throw std::runtime_error ("source_file value unexpected in "
				+ to_string (attr));

    case dwarf::VS_discr_list:
      return DW_FORM_block;
    }

  throw std::logic_error ("strange value_space");
}

inline void
dwarf_output_collector::shape_type::hashnadd (int name, int form)
{
  _m_attrs.insert (std::make_pair (name, form));
}

inline
dwarf_output_collector::shape_type::shape_type (die_map::value_type const &emt)
  : _m_tag (emt.first.tag ())
  , _m_has_children (emt.first.has_children ())
  , _m_hash (8675309 << _m_has_children)
{
  if (emt.second.with_sibling && emt.first.has_children ())
    hashnadd (DW_AT_sibling, DW_FORM_ref_udata);

  for (die_type::attributes_type::const_iterator it
	 = emt.first.attributes ().begin ();
       it != emt.first.attributes ().end (); ++it)
    hashnadd (it->first, attr_form (_m_tag, *it));

  // Make sure the hash is computed based on canonical order of
  // (unique) attributes, not based on order in which the attributes
  // are in DIE.
  for (attrs_type::const_iterator it = _m_attrs.begin ();
       it != _m_attrs.end (); ++it)
    {
      subr::hash_combine (_m_hash, it->first);
      subr::hash_combine (_m_hash, it->second);
    }
}

void
dwarf_output_collector::shape_info::instantiate
    (dwarf_output_collector::shape_type const &shape)
{
  // For now create single instance with the canonical forms.
  _m_instances.push_back (instance_type ());
  instance_type &inst = _m_instances[0];
  for (shape_type::attrs_type::const_iterator it = shape._m_attrs.begin ();
       it != shape._m_attrs.end (); ++it)
    inst.first.push_back (it->second);

  for (die_ref_vect::iterator it = _m_users.begin ();
       it != _m_users.end (); ++it)
    _m_instance_map[*it] = _m_instances.begin ();
}

void
dwarf_output_collector::shape_info::build_data
    (dwarf_output_collector::shape_type const &shape,
     dwarf_output_collector::shape_info::instance_type const &inst,
     std::vector<uint8_t> &data)
{
  write_uleb128 (data, inst.second);
  write_uleb128 (data, shape._m_tag);
  data.push_back (shape._m_has_children ? DW_CHILDREN_yes : DW_CHILDREN_no);

  // We iterate shape attribute map in parallel with instance
  // attribute forms.  They are stored in the same order.  We get
  // attribute name from attribute map, and concrete form from
  // instance.
  assert (shape._m_attrs.size () == inst.first.size ());
  dwarf_output_collector::shape_type::attrs_type::const_iterator at
    = shape._m_attrs.begin ();
  for (instance_type::first_type::const_iterator it = inst.first.begin ();
       it != inst.first.end (); ++it)
    {
      // ULEB128 name & form
      write_uleb128 (data, at++->first);
      write_uleb128 (data, *it);
    }

  data.push_back (0);
  data.push_back (0);
}

void
dwarf_output_collector::build_output ()
{
  for (die_map::const_iterator it = _m_unique.begin ();
       it != _m_unique.end (); ++it)
    {
      shape_type shape (*it);
      shape_map::iterator st = _m_shapes.find (shape);
      if (st != _m_shapes.end ())
	st->second.add_user (&it->first);
      else
	_m_shapes.insert (std::make_pair (shape_type (*it), shape_info (*it)));
    }

  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    it->second.instantiate (it->first);

  std::cout << "shapes" << std::endl;
  size_t code = 0;
  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    {
      std::cout << "  " << it->first._m_tag;
      for (shape_type::attrs_type::const_iterator jt
	     = it->first._m_attrs.begin ();
	   jt != it->first._m_attrs.end (); ++jt)
	std::cout << " " << dwarf_attr_string (jt->first)
		  << ":" << dwarf_form_string (jt->second);
      std::cout << std::endl;

      for (shape_info::instances_type::iterator jt
	     = it->second._m_instances.begin ();
	   jt != it->second._m_instances.end (); ++jt)
	{
	  jt->second = ++code;
	  std::cout << "    i" << jt->second;
	  for (shape_info::instance_type::first_type::const_iterator kt
		 = jt->first.begin ();  kt != jt->first.end (); ++kt)
	    std::cout << " " << dwarf_form_string (*kt);
	  std::cout << std::endl;
	}

      for (die_ref_vect::const_iterator jt = it->second._m_users.begin ();
	   jt != it->second._m_users.end (); ++jt)
	std::cout << "    " << to_string (**jt)
		  << "; i" << (it->second._m_instance_map[*jt]->second)
		  << std::endl;
    }

  _m_output_built = true;
}

void
dwarf_output_collector::output_debug_abbrev (std::vector <uint8_t> &data
					     __attribute__ ((unused)))
{
  if (!_m_output_built)
    build_output ();

  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    for (shape_info::instances_type::const_iterator jt
	   = it->second._m_instances.begin ();
	 jt != it->second._m_instances.end (); ++jt)
      it->second.build_data (it->first, *jt, data);
}
