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
#include <cstdarg>
#include <byteswap.h>
#include "dwarf_output"
#include "../../src/dwarfstrings.h"

using namespace elfutils;

#define __unused __attribute__ ((unused))

namespace
{
  struct null_constraint_t
    : public dwarf_output_collector::shape_type::form_constraint_t
  {
    virtual bool satisfied (__unused dwarf_output::debug_info_entry const &die,
			    __unused int attr,
			    __unused dwarf_output::attr_value const &value) const
    {
      return true;
    }

    virtual bool equal (form_constraint_t const *other) const;
  } null_constraint;

  bool
  null_constraint_t::equal (form_constraint_t const *other) const
  {
    return other == &null_constraint;
  }


  struct cu_local_ref_constraint_t
    : public dwarf_output_collector::shape_type::form_constraint_t
  {
    virtual bool satisfied (__unused dwarf_output::debug_info_entry const &die,
			    __unused int attr,
			    __unused dwarf_output::attr_value const &value) const
    {
      return true; // xxx
    }

    virtual bool equal (form_constraint_t const *other) const;
  } cu_local_ref_constraint;

  bool
  cu_local_ref_constraint_t::equal (form_constraint_t const *other) const
  {
    return other == &cu_local_ref_constraint;
  }


  struct noreloc_constraint_t
    : public dwarf_output_collector::shape_type::form_constraint_t
  {
    virtual bool satisfied (__unused dwarf_output::debug_info_entry const &die,
			    __unused int attr,
			    __unused dwarf_output::attr_value const &value) const
    {
      return true; // xxx
    }

    virtual bool equal (form_constraint_t const *other) const;
  } noreloc_constraint;

  bool
  noreloc_constraint_t::equal (form_constraint_t const *other) const
  {
    return other == &noreloc_constraint;
  }


  struct constraint_and
    : public dwarf_output_collector::shape_type::form_constraint_t
  {
    form_constraint_t const *a;
    form_constraint_t const *b;

    constraint_and (form_constraint_t const *aa,
		    form_constraint_t const *bb)
      : a (aa), b (bb)
    {}

    virtual bool satisfied (dwarf_output::debug_info_entry const &die,
			    int attr,
			    dwarf_output::attr_value const &value) const
    {
      return a->satisfied (die, attr, value)
	&& b->satisfied (die, attr, value);
    }

    virtual bool equal (form_constraint_t const *other) const
    {
      if (constraint_and const *o
	    = dynamic_cast <constraint_and const *> (other))
	return (a->equal (o->a) && b->equal(o->b))
	  || (b->equal (o->a) && a->equal(o->b));
      else
	return false;
    }
  };
}

dwarf_output_collector::shape_type::candidate_form::candidate_form
    (int a_form, form_constraint_t const *a_constraint)
  : _m_hash (0) // xxx
  , form (a_form)
  , constraint (a_constraint ?: &null_constraint)
{}

namespace
{
  struct ref_forms_t
    : public dwarf_output_collector::shape_type::candidate_form_vec
  {
    ref_forms_t ()
    {
      typedef dwarf_output_collector::shape_type::candidate_form
	candidate_form;
      static constraint_and local_noreloc_constaint (&cu_local_ref_constraint,
						     &noreloc_constraint);
      add (DW_FORM_ref_addr);
      add (candidate_form (DW_FORM_ref8, &cu_local_ref_constraint));
      add (candidate_form (DW_FORM_ref4, &cu_local_ref_constraint));
      add (candidate_form (DW_FORM_ref2, &local_noreloc_constaint));
      add (candidate_form (DW_FORM_ref1, &local_noreloc_constaint));
      add (candidate_form (DW_FORM_ref_udata, &local_noreloc_constaint));
    }
  } ref_forms;

  inline dwarf_output_collector::shape_type::candidate_form_vec const &
  candidate_forms (int tag, int at_name, const dwarf_output::attr_value &value)
  {
    // import some types into the namespace
    typedef dwarf_output_collector::shape_type::candidate_form
      candidate_form;
    typedef dwarf_output_collector::shape_type::candidate_form_vec
      candidate_form_vec;
    typedef dwarf_output_collector::shape_type::form_constraint_t
      form_constraint_t;

    /* Having the most spacious form first means that simple sweep
       that picks the first suitable form picks that biggest one,
       meaning the form will be able to hold whatever data
       necessary.
       Having variable length the last means that in case of tie,
       fixed length forms that are easy to read win.  */
    static candidate_form_vec block_forms = candidate_form_vec ()
      .add (DW_FORM_block4)
      .add (DW_FORM_block2)
      .add (DW_FORM_block1)
      .add (DW_FORM_block);

    static candidate_form_vec const_forms = candidate_form_vec ()
      .add (DW_FORM_data8)
      .add (DW_FORM_data4)
      .add (DW_FORM_data2) // xxx noreloc
      .add (DW_FORM_data1) // xxx noreloc
      .add (DW_FORM_udata) // xxx noreloc, nopatch
      .add (DW_FORM_sdata);// xxx noreloc, nopatch

    static candidate_form_vec string_forms = candidate_form_vec ()
      .add (DW_FORM_strp)
      .add (DW_FORM_string);

    switch (value.what_space ())
      {
      case dwarf::VS_address:
	{
	  static candidate_form_vec forms = candidate_form_vec ()
	    .add(DW_FORM_addr);
	  return forms;
	}

      case dwarf::VS_flag:
	{
	  static candidate_form_vec forms = candidate_form_vec ()
	    .add (DW_FORM_flag)
	    .add (DW_FORM_flag_present);
	  return forms;
	}

      case dwarf::VS_reference:
	return ::ref_forms;

      case dwarf::VS_string:
      case dwarf::VS_identifier:
	/* xxx
	  - string: price is strlen+1 per DIE

	  - strp: constant price of strlen+1, then dwarf64?8:4 bytes per
          DIE that contains the same string (or some suffix of that
          string, in case libebl does the string consolidation)

	  - needs to be decided separately.  A string-collecting sweep
          over the DIE tree, then decide whether make it strp or
          string based on counts and strlen and price of separate
          abbrev.
	*/
	return string_forms;

      case dwarf::VS_constant:
	if (!value.constant_is_integer ())
	  return block_forms;
	/* Fall through.  */

      case dwarf::VS_dwarf_constant:
      case dwarf::VS_source_line:
      case dwarf::VS_source_column:
	return const_forms;

      case dwarf::VS_location:
	if (!value.location ().is_list ())
	  return block_forms;
	/* Fall through.  */

      case dwarf::VS_lineptr:
      case dwarf::VS_macptr:
      case dwarf::VS_rangelistptr:
	/* This can be either data4 or data8 depending on the size of
	   the offset to the client data that we are trying to encode.
	   In DWARF 4, the only available offset is DW_FORM_sec_offset,
	   which is 4 bytes in 32-bit dwarf and 8 bytes in 64-bit.  */
	{
	  static candidate_form_vec forms = candidate_form_vec ()
	    .add (DW_FORM_sec_offset) // xxx if it's not DWARF 4.0,
				      // encode this either as data8,
				      // or data4.
	    ;
	  return forms;
	}

      case dwarf::VS_source_file:
	switch (at_name)
	  {
	  case DW_AT_decl_file:
	  case DW_AT_call_file:
	    return const_forms;

	  case DW_AT_comp_dir:
	    return string_forms;

	  case DW_AT_name:
	    switch (tag)
	      {
	      case DW_TAG_compile_unit:
	      case DW_TAG_partial_unit:
		return string_forms;
	      }
	    break;
	  }
	throw std::runtime_error ("source_file value unexpected in "
				  + to_string (value));

      case dwarf::VS_discr_list:
	return block_forms;
      }

    throw std::logic_error ("strange value_space");
  }

  bool
  numerical_p (const dwarf_output::attr_value &value)
  {
    dwarf::value_space vs = value.what_space ();

    switch (vs)
      {
      case dwarf::VS_flag:
      case dwarf::VS_rangelistptr:
      case dwarf::VS_lineptr:
      case dwarf::VS_macptr:
      case dwarf::VS_location:
      case dwarf::VS_constant:
      case dwarf::VS_dwarf_constant:
      case dwarf::VS_source_line:
      case dwarf::VS_source_column:
      case dwarf::VS_address:
	return true;

      case dwarf::VS_string:
      case dwarf::VS_identifier:
      case dwarf::VS_source_file: // xxx or is it?
      case dwarf::VS_reference:   // xxx this one is numerical in
				  // principle, but optimizing
				  // references is fun on its own and
				  // for much later
      case dwarf::VS_discr_list:
	return false;
      }

    abort ();
  }

  uint64_t
  numerical_value_to_optimize (const dwarf_output::attr_value &value)
  {
    dwarf::value_space vs = value.what_space ();

    switch (vs)
      {
      case dwarf::VS_flag:
	return !!value.flag ();

      case dwarf::VS_rangelistptr:
      case dwarf::VS_lineptr:
      case dwarf::VS_macptr:
      case dwarf::VS_location:
	return 0; /* xxx */

      case dwarf::VS_constant:
	if (value.constant_is_integer ())
	  return value.constant ();
	else
	  return value.constant_block ().size ();

      case dwarf::VS_dwarf_constant:
	return value.dwarf_constant ();

      case dwarf::VS_source_line:
	return value.source_line ();

      case dwarf::VS_source_column:
	return value.source_column ();

      case dwarf::VS_address:
	return value.address ();

      case dwarf::VS_string:
      case dwarf::VS_identifier:
      case dwarf::VS_source_file:
      case dwarf::VS_reference:
      case dwarf::VS_discr_list:
	abort ();
      }

    abort ();
  }
}

dwarf_output_collector::shape_type::shape_type
    (die_map::value_type const &emt)
  : _m_tag (emt.first.tag ())
  , _m_has_children (emt.first.has_children ())
  , _m_hash (8675309 << _m_has_children)
{
  if (emt.second.with_sibling && emt.first.has_children ())
    _m_attrs.push_back (DW_AT_sibling);

  for (die_type::attributes_type::const_iterator it
	 = emt.first.attributes ().begin ();
       it != emt.first.attributes ().end (); ++it)
    _m_attrs.push_back (it->first);

  // Sort it, but leave sibling attribute at the beginning.
  std::sort (_m_attrs.begin () + 1, _m_attrs.end ());

  // Make sure the hash is computed based on canonical order of
  // (unique) attributes, not based on order in which the attributes
  // are in DIE.
  for (attrs_vec::const_iterator it = _m_attrs.begin ();
       it != _m_attrs.end (); ++it)
    subr::hash_combine (_m_hash, *it);
}

namespace
{
  struct abbreviation
  {
    dwarf_output_collector::die_ref_vect _m_users;
    std::vector <int> _m_forms;

    abbreviation (std::vector <int> forms)
      : _m_forms (forms)
    {}
  };
}

void
dwarf_output_collector::shape_info::add_user (die_type const *die)
{
  _m_users.push_back (die);
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

  template <class Iterator>
  void
  dw_write_sleb128 (Iterator it, int64_t value)
  {
    bool more = true;
    do
      {
	uint8_t byte = value & 0x7fULL;
	value >>= 7;
	if ((value == 0 && !(byte & 0x40))
	    || (value == -1 && (byte & 0x40)))
	  more = false;
	else
	  byte |= 0x80;

	*it++ = byte;
      }
    while (more);
  }

  template <int width> struct width_to_int;

  template <> struct width_to_int <0>
  {
    typedef uint8_t unsigned_t;
    static uint8_t bswap (uint8_t value) { return value; }
  };

  template <> struct width_to_int <1>
    : width_to_int <0>
  {};

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
      case 0:
	break;
      default:
	throw std::runtime_error ("Width has to be 0, 1, 2, 4 or 8.");
      }
  }

  void dw_write_64 (section_appender &appender, bool is_64,
		    uint64_t value, bool big_endian)
  {
    size_t w = is_64 ? 8 : 4;
    dw_write_var (appender.alloc (w), w, value, big_endian);
  }

  void dw_write_form (section_appender &appender, int form,
		      uint64_t value, bool big_endian,
		      bool addr_64, bool dwarf_64)
  {
    switch (form)
      {
      case DW_FORM_flag_present:
	return;

#define HANDLE_DATA_REF(W)					\
	case DW_FORM_data##W:					\
        case DW_FORM_ref##W:					\
	dw_write<W> (appender.alloc (W), value, big_endian);	\
	  return

      case DW_FORM_flag:
	assert (value == 1 || value == 0);
	/* fall through */
      HANDLE_DATA_REF (1);

      HANDLE_DATA_REF (2);
      HANDLE_DATA_REF (4);
      HANDLE_DATA_REF (8);

#undef HANDLE_DATA_REF

      case DW_FORM_addr:
	dw_write_64 (appender, addr_64, value, big_endian);
	return;

      case DW_FORM_ref_addr:
      case DW_FORM_strp:
      case DW_FORM_sec_offset:
	dw_write_64 (appender, dwarf_64, value, big_endian);
	return;

      case DW_FORM_udata:
      case DW_FORM_ref_udata:
      case DW_FORM_exprloc:
      case DW_FORM_indirect:
	dw_write_uleb128 (std::back_inserter (appender), value);
	return;

      case DW_FORM_sdata:
	dw_write_sleb128 (std::back_inserter (appender), value);
      }

    throw std::runtime_error (std::string ("Don't know how to write ")
			      + dwarf_form_string (form));
  }

  class CountingIterator
  {
    size_t &_m_count;

  public:
    CountingIterator (size_t &count)
      : _m_count (count)
    {}

    CountingIterator (CountingIterator const &copy)
      : _m_count (copy._m_count)
    {}

    CountingIterator &operator= (CountingIterator const &other)
    {
      _m_count = other._m_count;
      return *this;
    }

    CountingIterator &operator++ (int)
    {
      _m_count++;
      return *this;
    }

    struct ref
    {
      template <class T>
      ref &operator= (T t __attribute__ ((unused)))
      {
	return *this;
      }
    };

    ref operator *()
    {
      return ref ();
    }
  };

  /* Return width of data stored with given form.  For block forms,
     return width of length field.  */
  size_t
  form_width (int form, bool addr_64, bool dwarf_64)
  {
    switch (form)
      {
      case DW_FORM_flag_present:
	return 0;

      case DW_FORM_block1:
      case DW_FORM_data1:
      case DW_FORM_flag:
      case DW_FORM_ref1:
	return 1;

      case DW_FORM_block2:
      case DW_FORM_data2:
      case DW_FORM_ref2:
	return 2;

      case DW_FORM_block4:
      case DW_FORM_data4:
      case DW_FORM_ref4:
	return 4;

      case DW_FORM_data8:
      case DW_FORM_ref8:
      case DW_FORM_ref_sig8:
	return 8;

      case DW_FORM_addr:
	return addr_64 ? 8 : 4;

      case DW_FORM_ref_addr:
      case DW_FORM_strp:
      case DW_FORM_sec_offset:
	return dwarf_64 ? 8 : 4;

      case DW_FORM_block:
      case DW_FORM_sdata:
      case DW_FORM_udata:
      case DW_FORM_ref_udata:
      case DW_FORM_exprloc:
      case DW_FORM_indirect:
	throw std::runtime_error
	  (std::string ("Can't compute width of LEB128 form ")
	   + dwarf_form_string (form));

      case DW_FORM_string:
	throw std::runtime_error
	  ("You shouldn't need the width of DW_FORM_string.");
      }

    throw std::runtime_error
      (std::string ("Don't know length of ") + dwarf_form_string (form));
  }

  size_t
  numerical_encoded_length (uint64_t value, int form,
			    bool addr_64, bool dwarf_64)
  {
    switch (form)
      {
      case DW_FORM_udata:
      case DW_FORM_ref_udata:
      case DW_FORM_exprloc:
      case DW_FORM_indirect:
      case DW_FORM_block:
      case DW_FORM_sdata:
	{
	  size_t count = 0;
	  CountingIterator counter (count);
	  if (form == DW_FORM_sdata)
	    dw_write_sleb128 (counter, value);
	  else
	    dw_write_uleb128 (counter, value);
	  return count;
	}

      default:
	return form_width (form, addr_64, dwarf_64);
      }
  }

  bool
  numerical_value_fits_form (uint64_t value, int form,
			     bool addr_64, bool dwarf_64)
  {
    bool is_64;
    switch (form)
      {
      case DW_FORM_flag_present:
	return value == 1;

      case DW_FORM_flag:
      case DW_FORM_data1:
      case DW_FORM_ref1:
      case DW_FORM_block1:
	return value <= (uint8_t)-1;

      case DW_FORM_data2:
      case DW_FORM_ref2:
      case DW_FORM_block2:
	return value <= (uint16_t)-1;

      case DW_FORM_addr:
	is_64 = addr_64;
	if (false)
      case DW_FORM_ref_addr:
      case DW_FORM_strp:
      case DW_FORM_sec_offset:
	  is_64 = dwarf_64;
	if (is_64)
	  return true;
	else
      case DW_FORM_data4:
      case DW_FORM_ref4:
      case DW_FORM_block4:
	  return value <= (uint32_t)-1;

      case DW_FORM_data8:
      case DW_FORM_ref8:
      case DW_FORM_udata:
      case DW_FORM_ref_udata:
      case DW_FORM_exprloc:
      case DW_FORM_indirect:
      case DW_FORM_block:
      case DW_FORM_sdata:
	return true;
      }

    throw std::runtime_error
      (std::string ("Don't know whether value fits ")
       + dwarf_form_string (form));
  }
}

void
dwarf_output_collector::shape_info::instantiate
  (dwarf_output_collector::shape_type const &shape,
   bool addr_64, bool dwarf_64)
{
  for (die_ref_vect::const_iterator it = _m_users.begin ();
       it != _m_users.end (); ++it)
    {
      instance_type inst;
      die_type const &die = **it;
      die_type::attributes_type const &attribs = die.attributes ();
      for (shape_type::attrs_vec::const_iterator at = shape._m_attrs.begin ();
	   at != shape._m_attrs.end (); ++at)
	{
	  int name = *at;

	  // Optimization of sibling attribute will be done afterward.
	  // For now just stick the biggest form in there.
	  int form;
	  if (name == DW_AT_sibling)
	    form = DW_FORM_ref8;
	  else
	    {
	      dwarf_output::attr_value const &value
		= attribs.find (name)->second;
	      shape_type::candidate_form_vec const &candidates
		= ::candidate_forms (die.tag (), *at, value);

	      if (!numerical_p (value))
		{
		  form = -1;

		  // Just take the first form matching the
		  // constraints, we will optimize in separate sweep.
		  // Assume that candidates are stored in order from
		  // the most capacitous to the least.
		  for (shape_type::candidate_form_vec::const_iterator ct
			 = candidates.begin ();
		       ct != candidates.end (); ++ct)
		    if (ct->constraint->satisfied (die, name, value))
		      {
			form = ct->form;
			break;
		      }
		  assert (form != -1);
		}
	      else
		{
		  size_t best_len = 0;
		  form = -1;

		  uint64_t opt_val = numerical_value_to_optimize (value);

		  for (shape_type::candidate_form_vec::const_iterator ct
			 = candidates.begin ();
		       ct != candidates.end (); ++ct)
		    if (ct->constraint->satisfied (die, name, value)
			&& numerical_value_fits_form (opt_val, ct->form,
						      addr_64, dwarf_64))
		      {
			size_t len
			  = numerical_encoded_length (opt_val, ct->form,
						      addr_64, dwarf_64);
			if (form == -1 || len < best_len)
			  {
			    best_len = len;
			    form = ct->form;
			  }
		      }
		  assert (form != -1);
		}
	    }
	  inst.first.push_back (form);
	}
      _m_instance_map[*it] = _m_instances.size ();
      _m_instances.push_back (inst);
    }

  // xxx: instance merging?  Locate instances A and B (and C and ...?)
  // such that instance X can be created that covers all users of A
  // and B, and such that the space saved by removing the
  // abbreviations A and B tops the overhead of introducing non-ideal
  // (wrt users of A and B) X and moving all the users over to it.
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
  dwarf_output_collector::shape_type::attrs_vec::const_iterator at
    = shape._m_attrs.begin ();
  for (instance_type::first_type::const_iterator it = inst.first.begin ();
       it != inst.first.end (); ++it)
    {
      // ULEB128 name & form
      ::dw_write_uleb128 (inserter, *at++);
      ::dw_write_uleb128 (inserter, *it);
    }

  // 0 for name & form to terminate the abbreviation
  *inserter++ = 0;
  *inserter++ = 0;
}

void
dwarf_output_collector::build_output (bool addr_64, bool dwarf_64)
{
  size_t code = 0;
  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    {
      it->second.instantiate (it->first, addr_64, dwarf_64);
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
				   bool addr_64)
{
  if (!c._m_output_built)
    /* xxx we need to decide on dwarf_64 as soon as here.  That's not
       good.  We will probably have to build debug_info and
       abbreviations in parallel, and dump abbreviations after
       debug_info is done and we know how big the data were.  */
    c.build_output (addr_64, false /* xxx dwarf_64 */);

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
  if (_m_recomputer != NULL)
    value = _m_recomputer->recompute (value);
  ::dw_write_var (_m_ptr, _m_len, value, _m_big_endian);
}

struct local_ref_recomputer
  : public dwarf_output::gap::recomputer
{
  uint64_t _m_base_addr;
  local_ref_recomputer (uint64_t base_addr)
    : _m_base_addr (base_addr)
  {}
  virtual uint64_t recompute (uint64_t value)
  {
    return value - _m_base_addr;
  }
};

class dwarf_output::recursive_dumper
{
  dwarf_output_collector &c;
  section_appender &appender;
  strtab &debug_str;
  bool addr_64;
  die_off_map &die_off;
  die_backpatch_vec &die_backpatch;
  str_backpatch_vec &str_backpatch;
  bool big_endian;
  gap::recomputer::ptr cu_local_recomputer;

  recursive_dumper (recursive_dumper const &copy); // nocopy

public:
  recursive_dumper (dwarf_output_collector &a_c,
		    section_appender &a_appender,
		    strtab &a_debug_str,
		    bool a_addr_64,
		    die_off_map &a_die_off,
		    die_backpatch_vec &a_die_backpatch,
		    str_backpatch_vec &a_str_backpatch,
		    bool a_big_endian,
		    uint64_t cu_start)
    : c (a_c),
      appender (a_appender),
      debug_str (a_debug_str),
      addr_64 (a_addr_64),
      die_off (a_die_off),
      die_backpatch (a_die_backpatch),
      str_backpatch (a_str_backpatch),
      big_endian (a_big_endian),
      cu_local_recomputer (new local_ref_recomputer (cu_start))
  {}

  void dump (debug_info_entry const &die,
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
	    instance_id = instance_it->second;
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
    for (dwarf_output_collector::shape_type::attrs_vec::const_iterator
	   at = shape->_m_attrs.begin ();
	 at != shape->_m_attrs.end (); ++at)
      {
	int name = *at;
	int form = *form_it++;
	if (name == DW_AT_sibling)
	  {
	    // XXX in fact we want to handle this case just like any
	    // other CU-local reference.  So also use this below (or
	    // better reuse single piece of code).
	    size_t gap_size
	      = ::form_width (form, addr_64, false /* xxx dwarf_64 */);
	    sibling_gap = gap (appender, gap_size,
			       big_endian, cu_local_recomputer);
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
	    if (form == DW_FORM_flag)
	      *appender.alloc (1) = !!value.flag ();
	    else
	      assert (form == DW_FORM_flag_present);
	    break;

	  case dwarf::VS_rangelistptr:
	  case dwarf::VS_lineptr:
	  case dwarf::VS_macptr:
	    ::dw_write_form (appender, form, 0 /*xxx*/, big_endian,
			     addr_64, false /* dwarf_64 */);
	    break;

	  case dwarf::VS_constant:
	    switch (form)
	      {
	      case DW_FORM_udata:
		::dw_write_uleb128 (inserter, value.constant ());
		break;
	      case DW_FORM_block:
		{
		  const std::vector<uint8_t> &block = value.constant_block ();
		  ::dw_write_uleb128 (inserter, block.size ());
		  std::copy (block.begin (), block.end (), inserter);
		}
		break;
	      default:
		abort (); // xxx
	      }
	    break;

	  case dwarf::VS_dwarf_constant:
	    ::dw_write_form (appender, form, value.dwarf_constant (),
			     big_endian, addr_64, false /* dwarf_64 */);
	    break;

	  case dwarf::VS_source_line:
	    ::dw_write_form (appender, form, value.source_line (),
			     big_endian, addr_64, false /* dwarf_64 */);
	    break;

	  case dwarf::VS_source_column:
	    ::dw_write_form (appender, form, value.source_column (),
			     big_endian, addr_64, false /* dwarf_64 */);
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
	      ::dw_write_form (appender, form, 0 /*xxx*/, big_endian,
			       addr_64, false /* dwarf_64 */);
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
};

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
    c.build_output (addr_64, false /* dwarf_64 */);

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
			big_endian, cu_start)
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
