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
#include <byteswap.h>
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
      return DW_FORM_strp;

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
	  return DW_FORM_strp;

	case DW_AT_name:
	  switch (tag)
	    {
	    case DW_TAG_compile_unit:
	    case DW_TAG_partial_unit:
	      return DW_FORM_strp;
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

dwarf_output_collector::shape_type::shape_type (die_map::value_type const &emt)
  : _m_tag (emt.first.tag ())
  , _m_has_children (emt.first.has_children ())
  , _m_hash (8675309 << _m_has_children)
{
  if (emt.second.with_sibling && emt.first.has_children ())
    _m_attrs[DW_AT_sibling] = DW_FORM_ref4;

  for (die_type::attributes_type::const_iterator it
	 = emt.first.attributes ().begin ();
       it != emt.first.attributes ().end (); ++it)
    _m_attrs[it->first] = attr_form (_m_tag, *it);

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

namespace
{
  template <class Iterator>
  void
  dw_write_uleb128 (Iterator it, uint64_t value)
  {
    do
      {
	uint8_t byte = value & 0x7fULL;
	value >>= 7;
	if (value != 0)
	  byte |= 0x80;
	*it++ = byte;
      }
    while (value != 0);
  }

  template <int width> struct width_to_int;

  template <> struct width_to_int <1>
  {
    typedef uint8_t unsigned_t;
    static uint8_t bswap (uint8_t value) { return value; }
  };

  template <> struct width_to_int <2>
  {
    typedef uint16_t unsigned_t;
    static uint16_t bswap (uint16_t value) { return bswap_16 (value); }
  };

  template <> struct width_to_int <4>
  {
    typedef uint32_t unsigned_t;
    static uint32_t bswap (uint32_t value) { return bswap_32 (value); }
  };

  template <> struct width_to_int <8>
  {
    typedef uint64_t unsigned_t;
    static uint64_t bswap (uint64_t value) { return bswap_64 (value); }
  };

  template <int width, class Iterator>
  void dw_write (Iterator it,
		 typename width_to_int<width>::unsigned_t value,
		 bool big_endian)
  {
    if (big_endian)
      value = width_to_int<width>::bswap (value);

    for (int i = 0; i < width; ++i)
      {
	*it++ = (uint8_t)value & 0xffUL;
	value >>= 8;
      }
  }

  template <class Iterator>
  void dw_write_var (Iterator it, unsigned width,
		     uint64_t value, bool big_endian)
  {
    switch (width)
      {
      case 8:
	::dw_write<8> (it, value, big_endian);
	break;
      case 4:
	::dw_write<4> (it, value, big_endian);
	break;
      case 2:
	::dw_write<2> (it, value, big_endian);
	break;
      case 1:
	::dw_write<1> (it, value, big_endian);
	break;
      default:
	throw std::runtime_error ("Width has to be 1, 2, 4 or 8.");
      }
  }
}

void
dwarf_output_collector::shape_info::build_data
    (dwarf_output_collector::shape_type const &shape,
     dwarf_output_collector::shape_info::instance_type const &inst,
     section_appender &appender)
{
  std::back_insert_iterator<section_appender> inserter
    = std::back_inserter (appender);
  ::dw_write_uleb128 (inserter, inst.second);
  ::dw_write_uleb128 (inserter, shape._m_tag);
  *inserter++ = shape._m_has_children ? DW_CHILDREN_yes : DW_CHILDREN_no;

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
      ::dw_write_uleb128 (inserter, at++->first);
      ::dw_write_uleb128 (inserter, *it);
    }

  // 0 for name & form to terminate the abbreviation
  *inserter++ = 0;
  *inserter++ = 0;
}

void
dwarf_output_collector::build_output ()
{
  /*
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
  */

  size_t code = 0;
  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    {
      it->second.instantiate (it->first);
      for (shape_info::instances_type::iterator jt
	     = it->second._m_instances.begin ();
	   jt != it->second._m_instances.end (); ++jt)
	jt->second = ++code;
    }

  /*
  std::cout << "shapes" << std::endl;
  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    {
      std::cout << "  " << dwarf_tag_string (it->first._m_tag);
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
	  std::cout << "    i" << jt->second;
	  for (shape_info::instance_type::first_type::const_iterator kt
		 = jt->first.begin ();  kt != jt->first.end (); ++kt)
	    std::cout << " " << dwarf_form_string (*kt);
	  std::cout << std::endl;
	}

      for (die_ref_vect::const_iterator jt = it->second._m_users.begin ();
	   jt != it->second._m_users.end (); ++jt)
	{
	  std::cout << "    " << to_string (**jt)
		    << "; i" << (it->second._m_instance_map[*jt]->second)
		    << std::endl;
	  die_type::attributes_type const &ats = (**jt).attributes ();
	  for (die_type::attributes_type::const_iterator kt = ats.begin ();
	       kt != ats.end (); ++kt)
	    std::cout << "      " << dwarf_attr_string (kt->first) << std::endl;
	}
    }
  */

  _m_output_built = true;
}

void
dwarf_output::output_debug_abbrev (section_appender &appender,
				   dwarf_output_collector &c,
				   bool addr_64 __attribute__ ((unused)))
{
  if (!c._m_output_built)
    c.build_output ();

  for (dwarf_output_collector::shape_map::iterator it = c._m_shapes.begin ();
       it != c._m_shapes.end (); ++it)
    for (dwarf_output_collector::shape_info::instances_type::const_iterator jt
	   = it->second._m_instances.begin ();
	 jt != it->second._m_instances.end (); ++jt)
      it->second.build_data (it->first, *jt, appender);

  appender.push_back (0); // terminate table
}

void
dwarf_output::gap::patch (uint64_t value) const
{
  ::dw_write_var (_m_ptr, _m_len, value, _m_big_endian);
}

void
dwarf_output::recursive_dumper::dump (debug_info_entry const &die,
				      gap &sibling_gap,
				      unsigned level)
{
  static char const spaces[] =
    "                                                            "
    "                                                            "
    "                                                            ";
  static char const *tail = spaces + strlen (spaces);
  __attribute__ ((unused)) char const *pad = tail - level * 2;
  //std::cout << pad << "CHILD " << dwarf_tag_string (die.tag ());

  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);

  /* Find shape instance.  XXX We currently have to iterate
     through all the shapes.  Fix later.  */
  dwarf_output_collector::shape_type const *shape = NULL;
  dwarf_output_collector::shape_info const *info = NULL;
  size_t instance_id = (size_t)-1;

  for (dwarf_output_collector::shape_map::iterator st = c._m_shapes.begin ();
       st != c._m_shapes.end (); ++st)
    {
      dwarf_output_collector::shape_info::instance_map::const_iterator
	instance_it = st->second._m_instance_map.find (&die);
      if (instance_it != st->second._m_instance_map.end ())
	{
	  assert (shape == NULL && info == NULL);

	  shape = &st->first;
	  info = &st->second;
	  instance_id
	    = instance_it->second - st->second._m_instances.begin ();
	}
    }
  assert (shape != NULL && info != NULL);

  /* Record where the DIE begins.  */
  die_off [die.offset ()] = appender.size ();
  // xxx handle non-CU-local

  /* Our instance.  */
  dwarf_output_collector::shape_info::instance_type const &instance
    = info->_m_instances[instance_id];
  size_t code = instance.second;
  ::dw_write_uleb128 (inserter, code);

  //std::cout << " " << code << std::endl;

  /* Dump attribute values.  */
  debug_info_entry::attributes_type const &attribs = die.attributes ();
  std::vector<int>::const_iterator form_it = instance.first.begin ();
  for (dwarf_output_collector::shape_type::attrs_type::const_iterator
	 at = shape->_m_attrs.begin ();
       at != shape->_m_attrs.end (); ++at)
    {
      int name = at->first;
      int form = *form_it++;
      if (name == DW_AT_sibling)
	{
	  // XXX we emit DW_FORM_ref4.  That's CU-local.  But we do
	  // all back-patching in a section-wide addressing, so this
	  // will break for DWARF with more than one CU.
	  sibling_gap = gap (appender, 4, big_endian);
	  continue;
	}

      debug_info_entry::attributes_type::const_iterator
	vt = attribs.find (name);
      assert (vt != attribs.end ());

      attr_value const &value = vt->second;
      /*
	std::cout << pad
	<< "    " << dwarf_attr_string (name)
	<< ":" << dwarf_form_string (form)
	<< ":" << dwarf_form_string (at->second)
	<< ":" << value.to_string () << std::endl;
      */
      dwarf::value_space vs = value.what_space ();

      switch (vs)
	{
	case dwarf::VS_flag:
	  assert (form == DW_FORM_flag);
	  *appender.alloc (1) = !!value.flag ();
	  break;

	case dwarf::VS_rangelistptr:
	case dwarf::VS_lineptr:
	case dwarf::VS_macptr:
	  assert (form == DW_FORM_data4); // XXX temporary
	  // XXX leave out for now
	  ::dw_write<4> (appender.alloc (4), 0, big_endian);
	  break;

	case dwarf::VS_constant:
	  assert (form == DW_FORM_udata);
	  ::dw_write_uleb128 (inserter, value.constant ());
	  break;

	case dwarf::VS_dwarf_constant:
	  assert (form == DW_FORM_udata);
	  ::dw_write_uleb128 (inserter, value.dwarf_constant ());
	  break;

	case dwarf::VS_source_line:
	  assert (form == DW_FORM_udata);
	  ::dw_write_uleb128 (inserter, value.source_line ());
	  break;

	case dwarf::VS_source_column:
	  assert (form == DW_FORM_udata);
	  ::dw_write_uleb128 (inserter, value.source_column ());
	  break;

	case dwarf::VS_string:
	case dwarf::VS_identifier:
	case dwarf::VS_source_file:
	  if (vs != dwarf::VS_source_file
	      || form == DW_FORM_string
	      || form == DW_FORM_strp)
	    {
	      std::string const &str =
		vs == dwarf::VS_string
		? value.string ()
		: (vs == dwarf::VS_source_file
		   ? value.source_file ().name ()
		   : value.identifier ());

	      if (form == DW_FORM_string)
		{
		  std::copy (str.begin (), str.end (), inserter);
		  *inserter++ = 0;
		}
	      else
		{
		  assert (form == DW_FORM_strp);

		  // xxx dwarf_64
		  str_backpatch.push_back
		    (std::make_pair (gap (appender, 4, big_endian),
				     debug_str.add (str)));
		}
	    }
	  else if (vs == dwarf::VS_source_file
		   && form == DW_FORM_udata)
	    // XXX leave out for now
	    ::dw_write_uleb128 (inserter, 0);
	  break;

	case dwarf::VS_address:
	  {
	    assert (form == DW_FORM_addr);
	    size_t w = addr_64 ? 8 : 4;
	    ::dw_write_var (appender.alloc (w), w,
			    value.address (), big_endian);
	  }
	  break;

	case dwarf::VS_reference:
	  {
	    assert (form == DW_FORM_ref_addr);
	    // XXX dwarf64
	    die_backpatch.push_back
	      (std::make_pair (gap (appender, 4, big_endian),
			       value.reference ()->offset ()));
	  }
	  break;

	case dwarf::VS_location:
	  // XXX leave out for now
	  if (form == DW_FORM_block)
	    ::dw_write_uleb128 (inserter, 0);
	  else
	    {
	      assert (form == DW_FORM_data4); // XXX temporary
	      ::dw_write<4> (appender.alloc (4), 0, big_endian);
	    }
	  break;

	case dwarf::VS_discr_list:
	  throw std::runtime_error ("Can't handle VS_discr_list.");
	};
    }
  assert (form_it == instance.first.end ());

  if (!die.children ().empty ())
    {
      gap my_sibling_gap;
      for (compile_unit::children_type::const_iterator jt
	     = die.children ().begin (); jt != die.children ().end (); ++jt)
	{
	  if (my_sibling_gap.valid ())
	    {
	      die_backpatch.push_back (std::make_pair (my_sibling_gap,
						       jt->offset ()));
	      my_sibling_gap = gap ();
	    }
	  dump (*jt, my_sibling_gap, level + 1);
	}
      *inserter++ = 0;
    }
}

void
dwarf_output::output_debug_info (section_appender &appender,
				 dwarf_output_collector &c,
				 strtab &debug_str,
				 str_backpatch_vec &str_backpatch,
				 bool addr_64, bool big_endian)
{
  /* We request appender directly, because we depend on .alloc method
     being implemented, which is not the case for std containers.
     Alternative approach would use purely random access
     iterator-based solution, which would be considerable amount of
     work (and bugs) and would involve memory overhead for holding all
     these otherwise useless Elf_Data pointers that we've allocated in
     the past.  So just ditch it and KISS.  */

  if (!c._m_output_built)
    c.build_output ();

  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);

  for (compile_units::const_iterator it = _m_units.begin ();
       it != _m_units.end (); ++it)
    {
      // Remember where the unit started for back-patching of size.
      size_t cu_start = appender.size ();

      // Unit length.
      gap length_gap (appender, 4 /*XXX dwarf64*/, big_endian);

      // Version.
      ::dw_write<2> (appender.alloc (2), 3, big_endian);

      // Debug abbrev offset.  Use the single abbrev table that we
      // emit at offset 0.
      ::dw_write<4> (appender.alloc (4), 0, big_endian);

      // Size in bytes of an address on the target architecture.
      *inserter++ = addr_64 ? 8 : 4;

      die_off_map die_off;
      die_backpatch_vec die_backpatch;

      //std::cout << "UNIT " << it->_m_cu_die << std::endl;
      gap fake_gap;
      recursive_dumper (c, appender, debug_str, addr_64,
			die_off, die_backpatch, str_backpatch,
			big_endian)
	.dump (*it->_m_cu_die, fake_gap, 0);
      assert (!fake_gap.valid ());

      std::for_each (die_backpatch.begin (), die_backpatch.end (),
		     backpatch_die_ref (die_off));

      /* Back-patch length.  */
      size_t length = appender.size () - cu_start - 4; // -4 for length info. XXX dwarf64
      assert (length < (uint32_t)-1); // XXX temporary XXX dwarf64
      length_gap.patch (length);
    }
}
