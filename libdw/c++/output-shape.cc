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
#include <tr1/unordered_set>
#include <tr1/functional>
#include <sstream>
#include "dwarf_output"
#include "../../src/dwarfstrings.h"
#include "../../src/dwarf-opcodes.h"

using namespace elfutils;

#define __unused __attribute__ ((unused))

namespace
{
  struct null_constraint_t
    : public dwarf_output::shape_type::form_constraint_t
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
    : public dwarf_output::shape_type::form_constraint_t
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
    : public dwarf_output::shape_type::form_constraint_t
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
    : public dwarf_output::shape_type::form_constraint_t
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

dwarf_output::shape_type::candidate_form::candidate_form
    (int a_form, form_constraint_t const *a_constraint)
  : _m_hash (0) // xxx
  , form (a_form)
  , constraint (a_constraint ?: &null_constraint)
{}

namespace
{
  struct ref_forms_t
    : public dwarf_output::shape_type::candidate_form_vec
  {
    ref_forms_t ()
    {
      typedef dwarf_output::shape_type::candidate_form
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

  bool
  source_file_is_string (int tag, int attr)
  {
    switch (attr)
      {
      case DW_AT_decl_file:
      case DW_AT_call_file:
	return false;

      case DW_AT_comp_dir:
	return true;

      case DW_AT_name:
	switch (tag)
	  {
	  case DW_TAG_compile_unit:
	  case DW_TAG_partial_unit:
	    return true;
	  }
      }

    throw std::runtime_error ("can't decide whether source_file_is_string");
  }

  inline dwarf_output::shape_type::candidate_form_vec const &
  candidate_forms (int tag, int attr, const dwarf_output::attr_value &value)
  {
    // import some types into the namespace
    typedef dwarf_output::shape_type::candidate_form
      candidate_form;
    typedef dwarf_output::shape_type::candidate_form_vec
      candidate_form_vec;
    typedef dwarf_output::shape_type::form_constraint_t
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
      .add (candidate_form (DW_FORM_data2, &noreloc_constraint))
      .add (candidate_form (DW_FORM_data1, &noreloc_constraint))
      .add (candidate_form (DW_FORM_udata, &noreloc_constraint)) // xxx & nopatch constaint
      .add (candidate_form (DW_FORM_sdata, &noreloc_constraint));// xxx & nopatch constaint

    static candidate_form_vec string_forms = candidate_form_vec ()
      .add (DW_FORM_string)
      .add (DW_FORM_strp)
      ;

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
	    //.add (DW_FORM_flag_present) // xxx DWARF4
	    ;
	  return forms;
	}

      case dwarf::VS_reference:
	return ::ref_forms;

      case dwarf::VS_string:
      case dwarf::VS_identifier:
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
	    //.add (DW_FORM_sec_offset) // xxx DWARF4
	    .add (DW_FORM_data8)
	    .add (DW_FORM_data4)
	    ;
	  return forms;
	}

      case dwarf::VS_source_file:
	if (source_file_is_string (tag, attr))
	  return string_forms;
	else
	  return const_forms;

      case dwarf::VS_discr_list:
	return block_forms;
      }

    throw std::logic_error ("strange value_space");
  }

  bool
  numerical_p (int tag, int attr, const dwarf_output::attr_value &value)
  {
    dwarf::value_space vs = value.what_space ();

    switch (vs)
      {
      case dwarf::VS_flag:
      case dwarf::VS_macptr:
      case dwarf::VS_constant:
      case dwarf::VS_dwarf_constant:
      case dwarf::VS_source_line:
      case dwarf::VS_source_column:
      case dwarf::VS_address:

      // We can optimize strings here, too, we just take the length of
      // the string as the value to encode, and treat it specially.
      case dwarf::VS_string:
      case dwarf::VS_identifier:
	return true;

      case dwarf::VS_reference:   // xxx this one is numerical in
				  // principle, but optimizing
				  // references is fun on its own and
				  // for much later
      case dwarf::VS_discr_list:
      case dwarf::VS_rangelistptr: // xxx same here
	return false;

      case dwarf::VS_location:
	if (!value.location ().is_list ())
	  return true;
	else
	  return false; // xxx and here, too

      case dwarf::VS_source_file:
	if (source_file_is_string (tag, attr))
	  return true;
	else
	  return false; // xxx and here, too

      // Require that .debug_line is emitted before .debug_info.
      case dwarf::VS_lineptr:
	// xxx but we need to move numerical_value_to_optimize to
	// writer.
	return false;
      }

    abort ();
  }

  uint64_t
  numerical_value_to_optimize (int tag, int attr,
			       const dwarf_output::attr_value &value)
  {
    dwarf::value_space vs = value.what_space ();

    switch (vs)
      {
      case dwarf::VS_flag:
	return !!value.flag ();

      case dwarf::VS_macptr:
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

      case dwarf::VS_source_file:
	if (!source_file_is_string (tag, attr))
	  return 0; /* xxx */
	/* fall-through */

      case dwarf::VS_string:
      case dwarf::VS_identifier:
	return value.string ().size ();

      case dwarf::VS_rangelistptr:
      case dwarf::VS_reference:
      case dwarf::VS_discr_list:
      case dwarf::VS_lineptr:
	abort ();

      case dwarf::VS_location:
	if (!value.location ().is_list ())
	  return value.location ().location ().size ();
	else
	  abort ();
      }

    abort ();
  }
}

dwarf_output::shape_type::shape_type (const debug_info_entry &die,
				      const dwarf_output::die_info &info)
  : _m_tag (die.tag ())
  , _m_with_sibling (info._m_with_sibling)
  , _m_has_children (die.has_children ())
  , _m_hash (8675309 << _m_has_children)
{
  for (debug_info_entry::attributes_type::const_iterator it
	 = die.attributes ().begin ();
       it != die.attributes ().end (); ++it)
    _m_attrs.push_back (it->first);
  std::sort (_m_attrs.begin (), _m_attrs.end ());

  // Make sure the hash is computed based on canonical order of
  // (unique) attributes, not based on order in which the attributes
  // are in DIE.
  for (attrs_vec::const_iterator it = _m_attrs.begin ();
       it != _m_attrs.end (); ++it)
    subr::hash_combine (_m_hash, *it);
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

}
template <class Iterator>
void
dwarf_output::writer::write_var (Iterator it, unsigned width, uint64_t value)
{
  switch (width)
    {
    case 8:
      ::dw_write<8> (it, value, _m_config.big_endian);
      break;
    case 4:
      ::dw_write<4> (it, value, _m_config.big_endian);
      break;
    case 2:
      ::dw_write<2> (it, value, _m_config.big_endian);
      break;
    case 1:
      ::dw_write<1> (it, value, _m_config.big_endian);
    case 0:
      break;
    default:
      throw std::runtime_error ("Width has to be 0, 1, 2, 4 or 8.");
    }
}

// Check that the value fits into 32-bits if !dwarf_64.  If it
// doesn't throw an exception.  The client will then be able to
// restart the process with dwarf_64 == true.
void
dwarf_output::writer::assert_fits_32 (uint64_t value) const
{
  if (!_m_config.dwarf_64 && value > (uint64_t)(uint32_t)-1)
    throw dwarf_output::writer::dwarf_32_not_enough ();
}

dwarf_output::writer::configuration::configuration (bool a_big_endian,
						    bool a_addr_64,
						    bool a_dwarf_64)
  : big_endian (a_big_endian),
    addr_64 (a_addr_64),
    dwarf_64 (a_dwarf_64)
{}

template <class Iterator>
void
dwarf_output::writer::write_form (Iterator it, int form, uint64_t value)
{
  switch (form)
    {
    case DW_FORM_flag_present:
      return;

#define HANDLE_DATA_REF(W)				\
      case DW_FORM_data##W:				\
    case DW_FORM_ref##W:				\
      dw_write<W> (it, value, _m_config.big_endian);	\
    return

    case DW_FORM_flag:
      assert (value == 1 || value == 0);
    case DW_FORM_block1:
      HANDLE_DATA_REF (1);

    case DW_FORM_block2:
      HANDLE_DATA_REF (2);

    case DW_FORM_block4:
      HANDLE_DATA_REF (4);

      HANDLE_DATA_REF (8);

#undef HANDLE_DATA_REF

    case DW_FORM_addr:
      write_64 (it, _m_config.addr_64, value);
      return;

    case DW_FORM_ref_addr:
    case DW_FORM_strp:
    case DW_FORM_sec_offset:
      assert_fits_32 (value);
      write_64 (it, _m_config.dwarf_64, value);
      return;

    case DW_FORM_udata:
    case DW_FORM_ref_udata:
    case DW_FORM_exprloc:
    case DW_FORM_indirect:
      dw_write_uleb128 (it, value);
      return;

    case DW_FORM_sdata:
      dw_write_sleb128 (it, value);
      return;
    }

  throw std::runtime_error (std::string ("Don't know how to write ")
			    + dwarf_form_string (form));
}

template <class IteratorIn, class IteratorOut>
void
dwarf_output::writer::write_block (IteratorOut it, int form,
				   IteratorIn begin, IteratorIn end)
{
  assert (form == DW_FORM_block
	  || form == DW_FORM_block1
	  || form == DW_FORM_block2
	  || form == DW_FORM_block4);

  write_form (it, form, end - begin);
  std::copy (begin, end, it);
}

namespace
{
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

      // xxx string logic should be extracted and treated using
      // different pass
      case DW_FORM_string:
	return value + 1; /* For strings, we yield string length plus
			     terminating zero.  */

      default:
	return form_width (form, addr_64, dwarf_64);
      }
  }

  bool
  numerical_value_fits_form (uint64_t value, int form, bool addr_64)
  {
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

      case DW_FORM_ref_addr:
      case DW_FORM_strp:
      case DW_FORM_sec_offset:
	// We simply assume that these dwarf_64-dependent forms can
	// contain any value.  If dwarf_64==false && value > 32bit, we
	// throw an exception when we try to write that value.
	return true;

      case DW_FORM_string:
	// This can hold anything.
	return true;

      case DW_FORM_addr:
	if (addr_64)
	  return true;
	/* fall-through */
      case DW_FORM_data4:
      case DW_FORM_ref4:
      case DW_FORM_block4:
	return value <= (uint64_t)(uint32_t)-1;

      /* 64-bit forms.  Everything fits there.  */
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

  void
  write_version (section_appender &appender,
		 bool big_endian,
		 unsigned version)
  {
    ::dw_write<2> (appender.alloc (2), version, big_endian);
  }
}

void
dwarf_output::writer::write_string (std::string const &str,
				    int form,
				    section_appender &appender)
{
  if (form == DW_FORM_string)
    {
      std::copy (str.begin (), str.end (),
		 std::back_inserter (appender));
      appender.push_back (0);
    }
  else
    {
      assert (form == DW_FORM_strp);

      _m_str_backpatch.push_back
	(std::make_pair (gap (*this, appender, form),
			 _m_debug_str.add (str)));
    }
}

void
dwarf_output::shape_info::instantiate
  (dwarf_output::shape_type const &shape,
   dwarf_output_collector &col,
   bool addr_64, bool dwarf_64)
{
  bool with_sibling = shape._m_with_sibling[true] && shape._m_has_children;
  bool without_sibling = shape._m_with_sibling[false] || !with_sibling;

  struct
  {
    void operator () (instance_type &inst,
		      debug_info_entry const &die, int attr,
		      bool my_addr_64, bool my_dwarf_64)
    {
      // Optimization of sibling attribute will be done afterward.
      // For now just stick the biggest form in there.
      int form;
      if (attr == DW_AT_sibling)
	form = DW_FORM_ref8;
      else
	{
	  int tag = die.tag ();
	  dwarf_output::attr_value const &value
	    = die.attributes ().find (attr)->second;
	  shape_type::candidate_form_vec const &candidates
	    = ::candidate_forms (tag, attr, value);

	  if (!numerical_p (tag, attr, value))
	    {
	      form = -1;

	      // Just take the first form matching the
	      // constraints, we will optimize in separate sweep.
	      // Assume that candidates are stored in order from
	      // the most capacitous to the least.
	      for (shape_type::candidate_form_vec::const_iterator ct
		     = candidates.begin ();
		   ct != candidates.end (); ++ct)
		if (ct->constraint->satisfied (die, attr, value))
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

	      uint64_t opt_val
		= numerical_value_to_optimize (tag, attr, value);

	      for (shape_type::candidate_form_vec::const_iterator ct
		     = candidates.begin ();
		   ct != candidates.end (); ++ct)
		if (ct->constraint->satisfied (die, attr, value)
		    && numerical_value_fits_form (opt_val, ct->form,
						  my_addr_64))
		  {
		    size_t len
		      = numerical_encoded_length (opt_val, ct->form,
						  my_addr_64, my_dwarf_64);
		    if (form == -1 || len < best_len)
		      {
			form = ct->form;
			if (len == 1) // you can't top that
			  break;
			best_len = len;
		      }
		  }
	      assert (form != -1);
	    }
	}
      inst.forms.insert (std::make_pair (attr, form));
    }
  } handle_attrib;

  for (die_ref_vect::const_iterator it = _m_users.begin ();
       it != _m_users.end (); ++it)
    {
      instance_type inst;
      debug_info_entry const &die = *it;
      for (shape_type::attrs_vec::const_iterator at = shape._m_attrs.begin ();
	   at != shape._m_attrs.end (); ++at)
	handle_attrib (inst, die, *at, addr_64, dwarf_64);

      die_info &i = col._m_unique.find (die)->second;
      if (without_sibling)
	i.abbrev_ptr[false] = _m_instances.insert (inst).first;

      if (with_sibling)
	{
	  handle_attrib (inst, die, DW_AT_sibling, addr_64, dwarf_64);
	  i.abbrev_ptr[true] = _m_instances.insert (inst).first;
	}
    }

  // xxx: instance merging?  Locate instances A and B (and C and ...?)
  // such that instance X can be created that covers all users of A
  // and B, and such that the space saved by removing the
  // abbreviations A and B tops the overhead of introducing non-ideal
  // (wrt users of A and B) X and moving all the users over to it.

  // Hmm, except that the is-advantageous math involves number of
  // users of the abbrev, and we don't know number of users before we
  // do the dissection of the tree to imported partial units.
}

void
dwarf_output::shape_info::build_data
    (dwarf_output::shape_type const &shape,
     dwarf_output::shape_info::instance_type const &inst,
     section_appender &appender)
{
  std::back_insert_iterator<section_appender> inserter
    = std::back_inserter (appender);
  ::dw_write_uleb128 (inserter, inst.code);
  ::dw_write_uleb128 (inserter, shape._m_tag);
  *inserter++ = shape._m_has_children ? DW_CHILDREN_yes : DW_CHILDREN_no;

  for (instance_type::forms_type::const_iterator it = inst.forms.begin ();
       it != inst.forms.end (); ++it)
    {
      // ULEB128 name & form
      ::dw_write_uleb128 (inserter, it->first);
      ::dw_write_uleb128 (inserter, it->second);
    }

  // 0 for name & form to terminate the abbreviation
  *inserter++ = 0;
  *inserter++ = 0;
}

dwarf_output::writer::gap::gap (writer &parent)
  : _m_parent (parent),
    _m_ptr (NULL)
{}

dwarf_output::writer::gap::gap (writer &parent, section_appender &appender,
				int form, uint64_t base)
  : _m_parent (parent),
    _m_ptr (appender.alloc (::form_width (form, parent._m_config.addr_64,
					  parent._m_config.dwarf_64))),
    _m_form (form),
    _m_base (base)
{}

dwarf_output::writer::gap::gap (writer &parent, unsigned char *ptr,
				int form, uint64_t base)
  : _m_parent (parent),
    _m_ptr (ptr),
    _m_form (form),
    _m_base (base)
{}

dwarf_output::writer::gap &
dwarf_output::writer::gap::operator= (gap const &other)
{
  assert (&_m_parent == &other._m_parent);
  _m_ptr = other._m_ptr;
  _m_form = other._m_form;
  _m_base = other._m_base;
  return *this;
}

void
dwarf_output::writer::gap::patch (uint64_t value) const
{
  _m_parent.write_form (_m_ptr, _m_form, value - _m_base);
}

namespace
{
  template <class Visitor, class die_info_pair>
  void
  traverse_die_tree (Visitor &visitor,
		     die_info_pair const &top_info_pair)
  {
    class recursive_traversal
    {
      Visitor &visitor;

    public:
      recursive_traversal (Visitor &a_visitor)
	: visitor (a_visitor)
      {
      }

      void traverse (typename Visitor::step_t &step,
		     die_info_pair const &info_pair,
		     bool has_sibling)
      {
	dwarf_output::debug_info_entry const &die = info_pair.first;

	visitor.visit_die (info_pair, step, has_sibling);
	if (!die.children ().empty ())
	  {
	    typename Visitor::step_t my_step (visitor, info_pair, &step);
	    my_step.before_children ();
	    for (typename std::vector<die_info_pair *>::const_iterator jt
		   = die.children ().info ().begin (); ;)
	      {
		die_info_pair const &dip = **jt++;
		bool my_has_sibling = jt != die.children ().info ().end ();
		my_step.before_recursion ();
		traverse (my_step, dip, my_has_sibling);
		my_step.after_recursion ();
		if (!my_has_sibling)
		  break;
	      }
	    my_step.after_children ();
	  }
      }
    };

    visitor.before_traversal ();
    typename Visitor::step_t step (visitor, top_info_pair, NULL);
    recursive_traversal (visitor)
      .traverse (step, top_info_pair, false);
    visitor.after_traversal ();
  }
}

class dwarf_output::writer::dump_die_tree
{
  // [(gap, die offset)]
  typedef std::vector<std::pair<gap, ::Dwarf_Off>> die_backpatch_vec;

  writer &_m_parent;
  section_appender &appender;
  die_off_map die_off;
  die_backpatch_vec die_backpatch;
  uint64_t _m_cu_start;
  size_t level;
  line_info_table const *lines;

  static const bool debug = false;

public:
  class step_t
  {
    dump_die_tree &_m_dumper;

  public:
    gap sibling_gap;

    step_t (dump_die_tree &dumper,
	    __unused die_info_pair const &info_pair,
	    __unused step_t *previous)
      : _m_dumper (dumper),
	sibling_gap (dumper._m_parent)
    {
      ++_m_dumper.level;
    }

    ~step_t ()
    {
      --_m_dumper.level;
    }

    void before_children () {}
    void after_recursion () {}

    void after_children ()
    {
      _m_dumper.appender.push_back (0);
    }

    void before_recursion ()
    {
      if (sibling_gap.valid ())
	{
	  sibling_gap.patch (_m_dumper.appender.size ());
	  sibling_gap = gap (_m_dumper._m_parent);
	}
    }
  };
  friend class step_t;

  dump_die_tree (writer &writer,
		 section_appender &a_appender,
		 uint64_t cu_start)
    : _m_parent (writer),
      appender (a_appender),
      _m_cu_start (cu_start),
      level (0),
      lines (NULL)
  {
  }

  void visit_die (dwarf_output::die_info_pair const &info_pair,
		  step_t &step,
		  bool has_sibling)
  {
    static char const spaces[] =
      "                                                            "
      "                                                            "
      "                                                            ";
    static char const *tail = spaces + strlen (spaces);
    char const *pad = tail - level * 2;

    debug_info_entry const &die = info_pair.first;
    die_info const &info = info_pair.second;
    int tag = die.tag ();

    std::back_insert_iterator <section_appender> inserter
      = std::back_inserter (appender);

    /* Record where the DIE begins.  */
    // xxx in fact, we can meet "the same" DIE several times in the
    // tree.  But since they are all equal, it doesn't matter which
    // one we end up resolving our references to.  Except for
    // siblings, which we handle differently.
    die_off [die.offset ()] = appender.size ();
    if (debug)
      std::cout << pad << "CHILD " << dwarf_tag_string (die.tag ())
		<< " [0x" << std::hex << die_off [die.offset ()] << std::dec << "]"
		<< " " << std::flush;

    /* Our instance.  */
    die_info::abbrev_ptr_map::const_iterator xt
      = info.abbrev_ptr.find (die.has_children () && has_sibling);
    assert (xt != info.abbrev_ptr.end ());
    shape_info::instance_type const &instance = *xt->second;
    ::dw_write_uleb128 (inserter, instance.code);

    if (debug)
      std::cout << " " << instance.code << std::endl;

    /* Dump attribute values.  */
    debug_info_entry::attributes_type const &attribs = die.attributes ();
    for (shape_info::instance_type::forms_type::const_iterator
	   at = instance.forms.begin (); at != instance.forms.end (); ++at)
      {
	int attr = at->first;
	int form = at->second;
	if (attr == DW_AT_sibling)
	  {
	    if (debug)
	      std::cout << pad << "    " << dwarf_attr_string (attr)
			<< ":" << dwarf_form_string (form)
			<< " sibling=" << info._m_with_sibling[false]
			<< ":" << info._m_with_sibling[true]
			<< std::endl;
	    step.sibling_gap = gap (_m_parent, appender, form, _m_cu_start);
	    continue;
	  }

	debug_info_entry::attributes_type::const_iterator
	  vt = attribs.find (attr);
	assert (vt != attribs.end ());

	attr_value const &value = vt->second;
	if (false && debug)
	  std::cout << ":" << value.to_string () << std::endl;

	dwarf::value_space vs = value.what_space ();

	switch (vs)
	  {
	  case dwarf::VS_flag:
	    if (form == DW_FORM_flag)
	      *appender.alloc (1) = !!value.flag ();
	    else
	      assert (form == DW_FORM_flag_present);
	    break;

	  case dwarf::VS_lineptr:
	    {
	      if (lines != NULL)
		throw std::runtime_error
		  ("Another DIE with lineptr attribute?!");

	      lines = &value.line_info ();
	      line_offsets_map::const_iterator it
		= _m_parent._m_line_offsets.find (lines);
	      if (it == _m_parent._m_line_offsets.end ())
		throw std::runtime_error
		  ("Emit .debug_line before .debug_info");
	      _m_parent.write_form (inserter, form, it->second.table_offset);
	    }
	    break;

	  case dwarf::VS_rangelistptr:
	    _m_parent._m_range_backpatch.push_back
	      (std::make_pair (gap (_m_parent, appender, form),
			       &value.ranges ()));
	    break;

	  case dwarf::VS_macptr:
	    _m_parent.write_form (inserter, form, 0 /*xxx*/);
	    break;

	  case dwarf::VS_constant:
	    if (value.constant_is_integer ())
	      _m_parent.write_form (inserter, form, value.constant ());
	    else
	      _m_parent.write_block (inserter, form,
				     value.constant_block ().begin (),
				     value.constant_block ().end ());
	    break;

	  case dwarf::VS_dwarf_constant:
	    _m_parent.write_form (inserter, form, value.dwarf_constant ());
	    break;

	  case dwarf::VS_source_line:
	    _m_parent.write_form (inserter, form, value.source_line ());
	    break;

	  case dwarf::VS_source_column:
	    _m_parent.write_form (inserter, form, value.source_column ());
	    break;

	  case dwarf::VS_string:
	    _m_parent.write_string (value.string (), form, appender);
	    break;

	  case dwarf::VS_identifier:
	    _m_parent.write_string (value.identifier (), form, appender);
	    break;

	  case dwarf::VS_source_file:
	    if (source_file_is_string (tag, attr))
	      _m_parent.write_string (value.source_file ().name (),
				      form, appender);
	    else
	      {
		assert (lines != NULL);
		writer::line_offsets_map::const_iterator
		  it = _m_parent._m_line_offsets.find (lines);
		assert (it != _m_parent._m_line_offsets.end ());
		writer::line_offsets::source_file_map::const_iterator
		  jt = it->second.source_files.find (value.source_file ());
		assert (jt != it->second.source_files.end ());
		_m_parent.write_form (inserter, form, jt->second);
	      }
	    break;

	  case dwarf::VS_address:
	    _m_parent.write_form (inserter, form, value.address ());
	    break;

	  case dwarf::VS_reference:
	    {
	      assert (form == DW_FORM_ref_addr);
	      die_backpatch.push_back
		(std::make_pair (gap (_m_parent, appender, form),
				 value.reference ()->offset ()));
	    }
	    break;

	  case dwarf::VS_location:
	    if (!value.location ().is_list ())
	      _m_parent.write_block (inserter, form,
				     value.location ().location ().begin (),
				     value.location ().location ().end ());
	    else
	      _m_parent._m_loc_backpatch.push_back
		(std::make_pair (gap (_m_parent, appender, form),
				 &value.location ()));
	    break;

	  case dwarf::VS_discr_list:
	    throw std::runtime_error ("Can't handle VS_discr_list.");
	  };
      }
  }

  void before_traversal () {}

  void after_traversal ()
  {
    for (die_backpatch_vec::const_iterator it = die_backpatch.begin ();
	 it != die_backpatch.end (); ++it)
      {
	die_off_map::const_iterator jt = die_off.find (it->second);
	if (jt == die_off.end ())
	  std::cout << "can't find offset " << it->second << std::endl;
	else
	  {
	    assert (jt != die_off.end ());
	    it->first.patch (jt->second);
	  }
      }
  }
};

dwarf_output::writer::writer (dwarf_output_collector &col,
			      dwarf_output &dw,
			      bool big_endian, bool addr_64, bool dwarf_64,
			      strtab &debug_str)
  : _m_config (big_endian, addr_64, dwarf_64),
    _m_col (col),
    _m_dw (dw),
    _m_debug_str (debug_str)
{
  size_t code = 0;
  for (dwarf_output_collector::die_map::const_iterator it
	 = col._m_unique.begin ();
       it != col._m_unique.end (); ++it)
    {
      dwarf_output::shape_type shape (it->first, it->second);
      shape_map::iterator st = _m_shapes.find (shape);
      if (st != _m_shapes.end ())
	st->second.add_user (it->first);
      else
	{
	  dwarf_output::shape_info info (it->first);
	  _m_shapes.insert (std::make_pair (shape, info));
	}
    }

  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    {
      it->second.instantiate (it->first, col, addr_64, dwarf_64);
      for (dwarf_output::shape_info::instance_set::iterator jt
	     = it->second._m_instances.begin ();
	   jt != it->second._m_instances.end (); ++jt)
	jt->code = ++code;
    }
}

void
dwarf_output::writer::output_debug_abbrev (section_appender &appender)
{
  for (shape_map::iterator it = _m_shapes.begin ();
       it != _m_shapes.end (); ++it)
    for (dwarf_output::shape_info::instance_set::iterator jt
	   = it->second._m_instances.begin ();
	 jt != it->second._m_instances.end (); ++jt)
      it->second.build_data (it->first, *jt, appender);

  appender.push_back (0); // terminate table
}

class dwarf_output::writer::length_field
{
  writer &_m_parent;
  elfutils::section_appender &_m_appender;
  const gap _m_length_gap;
  const size_t _m_cu_start;
  bool _m_finished;

  gap alloc_gap (elfutils::section_appender &appender) const
  {
    if (_m_parent._m_config.dwarf_64)
      ::dw_write<4> (appender.alloc (4), -1, _m_parent._m_config.big_endian);
    gap g (_m_parent, appender, DW_FORM_ref_addr);
    g.set_base (appender.size ());
    return g;
  }

public:
  length_field (writer &parent,
		elfutils::section_appender &appender)
    : _m_parent (parent),
      _m_appender (appender),
      _m_length_gap (alloc_gap (appender)),
      _m_cu_start (appender.size ()),
      _m_finished (false)
  {}

  void finish ()
  {
    assert (!_m_finished);
    _m_length_gap.patch (_m_appender.size ());
    _m_finished = true;
  }
};

void
dwarf_output::writer::output_debug_info (section_appender &appender)
{
  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);

  for (compile_units::const_iterator it = _m_dw._m_units.begin ();
       it != _m_dw._m_units.end (); ++it)
    {
      // Remember where the unit started for DIE offset calculation.
      size_t cu_start = appender.size ();

      length_field lf (*this, appender);
      ::write_version (appender, _m_config.big_endian, 3);

      // Debug abbrev offset.  Use the single abbrev table that we
      // emit at offset 0.
      ::dw_write<4> (appender.alloc (4), 0, _m_config.big_endian);

      // Size in bytes of an address on the target architecture.
      *inserter++ = _m_config.addr_64 ? 8 : 4;

      dump_die_tree dumper (*this, appender, cu_start);
      ::traverse_die_tree (dumper, *_m_col._m_unique.find (*it));

      lf.finish ();
    }
}

class dwarf_output::writer::linenum_prog_instruction
{
  writer &_m_parent;
  std::vector<int> const &_m_operands;
  std::vector<int>::const_iterator _m_op_it;

protected:
  std::vector<uint8_t> _m_buf;

  linenum_prog_instruction (writer &parent,
			    std::vector<int> const &operands)
    : _m_parent (parent),
      _m_operands (operands),
      _m_op_it (_m_operands.begin ())
  {}

public:
  void arg (uint64_t value)
  {
    assert (_m_op_it != _m_operands.end ());
    _m_parent.write_form (std::back_inserter (_m_buf), *_m_op_it++, value);
  }

  void arg (std::string const &value)
  {
    assert (_m_op_it != _m_operands.end ());
    int form = *_m_op_it++;
    assert (form == DW_FORM_string);

    std::copy (value.begin (), value.end (), std::back_inserter (_m_buf));
    _m_buf.push_back (0);
  }

  void write (section_appender &appender)
  {
    assert (_m_op_it == _m_operands.end ());
    std::copy (_m_buf.begin (), _m_buf.end (),
	       std::back_inserter (appender));
  }
};

class dwarf_output::writer::standard_opcode
  : public dwarf_output::writer::linenum_prog_instruction
{
  int _m_opcode;

  static std::vector<int> const &build_arglist (int opcode)
  {
    static struct arglist
      : public std::map<int, std::vector<int> >
    {
      arglist ()
      {
#define DW_LNS_0(OP)				\
	(*this)[OP];
#define DW_LNS_1(OP, OP1)			\
	(*this)[OP].push_back (OP1);

	DW_LNS_OPERANDS;

#undef DW_LNS_1
#undef DW_LNS_0
      }
    } const operands;

    arglist::const_iterator it = operands.find (opcode);
    assert (it != operands.end ());
    return it->second;
  }

public:
  standard_opcode (writer &parent, int opcode)
    : linenum_prog_instruction (parent, build_arglist (opcode)),
      _m_opcode (opcode)
  {}

  template <class T>
  inline standard_opcode &arg (T const &value)
  {
    linenum_prog_instruction::arg (value);
    return *this;
  }

  void write (section_appender &appender)
  {
    appender.push_back (_m_opcode);
    linenum_prog_instruction::write (appender);
  }
};

class dwarf_output::writer::extended_opcode
  : public dwarf_output::writer::linenum_prog_instruction
{
  int _m_opcode;

  static std::vector<int> const &build_arglist (int opcode)
  {
    static struct arglist
      : public std::map<int, std::vector<int> >
    {
      arglist ()
      {
#define DW_LNE_0(OP)				\
	(*this)[OP];
#define DW_LNE_1(OP, OP1)			\
	(*this)[OP].push_back (OP1);
#define DW_LNE_4(OP, OP1, OP2, OP3, OP4)	\
	(*this)[OP].push_back (OP1);		\
	(*this)[OP].push_back (OP2);		\
	(*this)[OP].push_back (OP3);		\
	(*this)[OP].push_back (OP4);

	DW_LNE_OPERANDS;

#undef DW_LNE_4
#undef DW_LNE_1
#undef DW_LNE_0
      }
    } const operands;

    arglist::const_iterator it = operands.find (opcode);
    assert (it != operands.end ());
    return it->second;
  }

public:
  extended_opcode (writer &parent, int opcode)
    : linenum_prog_instruction (parent, build_arglist (opcode)),
      _m_opcode (opcode)
  {}

  template <class T>
  inline extended_opcode &arg (T const &value)
  {
    linenum_prog_instruction::arg (value);
    return *this;
  }

  void write (section_appender &appender)
  {
    appender.push_back (0);
    ::dw_write_uleb128 (std::back_inserter (appender), _m_buf.size () + 1);
    appender.push_back (_m_opcode);
    linenum_prog_instruction::write (appender);
  }
};

class dwarf_output::writer::line_offsets::add_die_ref_files
{
  line_offsets::source_file_map &_m_files;

public:
  struct step_t
  {
    step_t (__unused add_die_ref_files &adder,
	    __unused die_info_pair const &info_pair,
	    __unused step_t *previous)
    {}

    void before_children () {}
    void after_children () {}
    void before_recursion () {}
    void after_recursion () {}
  };

  add_die_ref_files (line_offsets::source_file_map &files) : _m_files (files) {}

  void visit_die (dwarf_output::die_info_pair const &info_pair,
		  __unused step_t &step,
		  __unused bool has_sibling)
  {
    debug_info_entry const &die = info_pair.first;

    debug_info_entry::attributes_type const &attribs = die.attributes ();
    for (debug_info_entry::attributes_type::const_iterator at
	   = attribs.begin (); at != attribs.end (); ++at)
      {
	attr_value const &value = at->second;
	dwarf::value_space vs = value.what_space ();

	if (vs == dwarf::VS_source_file
	    && !source_file_is_string (die.tag (), at->first))
	  _m_files.insert (std::make_pair (value.source_file (), 0));
      }
  }

  void before_traversal () {}
  void after_traversal () {}
};

dwarf_output::writer::
line_offsets::line_offsets (__unused writer const &wr,
			    dwarf_output::line_table const &lines,
			    ::Dwarf_Off off)
  : table_offset (off)
{
  // We need to include all files referenced through DW_AT_*_file and
  // all files used in line number program.
  for (dwarf_output::line_table::const_iterator line_it = lines.begin ();
       line_it != lines.end (); ++line_it)
    {
      dwarf_output::line_entry const &entry = *line_it;
      dwarf_output::source_file const &file = entry.file ();
      line_offsets::source_file_map::const_iterator sfit
	= source_files.find (file);
      if (sfit == source_files.end ())
	source_files.insert (std::make_pair (file, 0));
    }

  for (compile_units::const_iterator it = wr._m_dw._m_units.begin ();
       it != wr._m_dw._m_units.end (); ++it)
    if (&it->lines () == &lines)
      {
	add_die_ref_files adder (source_files);
	::traverse_die_tree (adder, *wr._m_col._m_unique.find (*it));
      }

  // Assign numbers to source files.
  size_t file_idx = 0;
  for (line_offsets::source_file_map::iterator sfit = source_files.begin ();
       sfit != source_files.end (); ++sfit)
    sfit->second = ++file_idx;
}

void
dwarf_output::writer::output_debug_line (section_appender &appender)
{
  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);

  for (subr::value_set<dwarf_output::value::value_lineptr>::const_iterator it
	 = _m_col._m_line_info.begin ();
       it != _m_col._m_line_info.end (); ++it)
    {
      dwarf_output::line_info_table const &lt = it->second;
      dwarf_output::line_table const &lines = lt.lines ();

      line_offsets offset_tab (*this, lines, appender.size ());
      // the table is inserted in _m_line_offsets at the loop's end

      length_field table_length (*this, appender);
      ::write_version (appender, _m_config.big_endian, 3);
      length_field header_length (*this, appender);

      // minimum_instruction_length
      unsigned minimum_instruction_length = 1;
      appender.push_back (minimum_instruction_length);

      // default_is_stmt
      bool default_is_stmt = true;
      appender.push_back (default_is_stmt ? 1 : 0);

      // line_base, line_range
      appender.push_back (uint8_t (int8_t (0)));
      appender.push_back (1);

#define DW_LNS_0(OP) 0,
#define DW_LNS_1(OP, OP1) 1,
      uint8_t opcode_lengths[] = {
	DW_LNS_OPERANDS
      };
#undef DW_LNS_1
#undef DW_LNS_0

      // opcode_base
      appender.push_back (sizeof (opcode_lengths) + 1);

      // standard_opcode_lengths (array of ubyte)
      std::copy (opcode_lengths, opcode_lengths + sizeof (opcode_lengths),
		 inserter);

      // include_directories
      dwarf_output::directory_table const &dirs = lt.include_directories ();
      for (dwarf_output::directory_table::const_iterator dir_it
	     = dirs.begin (); dir_it != dirs.end (); ++dir_it)
	if (*dir_it != "")
	  {
	    std::copy (dir_it->begin (), dir_it->end (), inserter);
	    *inserter++ = 0;
	  }
      *inserter++ = 0;

      // file_names
      for (line_offsets::source_file_map::const_iterator sfit
	     = offset_tab.source_files.begin ();
	   sfit != offset_tab.source_files.end (); ++sfit)
	{
	  dwarf_output::source_file const &sf = sfit->first;

	  // Find the best-fitting directory for this filename.
	  size_t dir_index = 0;
	  size_t match_len = 0;
	  for (dwarf_output::directory_table::const_iterator dir_it
		 = dirs.begin () + 1; dir_it != dirs.end (); ++dir_it)
	    {
	      std::string const &dir = *dir_it;
	      if (dir.length () > match_len
		  && sf.name ().substr (0, dir.length ()) == dir)
		{
		  dir_index = dir_it - dirs.begin ();
		  match_len = dir.length ();
		}
	    }

	  std::string fn = sf.name ().substr (match_len + 1);
	  std::copy (fn.begin (), fn.end (), inserter);
	  *inserter++ = 0;
	  ::dw_write_uleb128 (inserter, dir_index);
	  ::dw_write_uleb128 (inserter, sf.mtime ());
	  ::dw_write_uleb128 (inserter, sf.size ());
	}
      *inserter++ = 0;

      header_length.finish ();

      // Now emit the "meat" of the table: the line number program.
      struct registers
      {
	::Dwarf_Addr addr;
	unsigned file;
	unsigned line;
	unsigned column;
	bool is_stmt;
	bool default_is_stmt;

	void init ()
	{
	  addr = 0;
	  file = 1;
	  line = 1;
	  column = 0;
	  is_stmt = default_is_stmt;
	}

	explicit registers (bool b)
	  : default_is_stmt (b)
	{
	  init ();
	}
      } reg (default_is_stmt);

      for (dwarf_output::line_table::const_iterator line_it = lines.begin ();
	   line_it != lines.end (); ++line_it)
	{
	  dwarf_output::line_entry const &entry = *line_it;
	  ::Dwarf_Addr addr = entry.address ();
	  unsigned file = offset_tab.source_files.find (entry.file ())->second;
	  unsigned line = entry.line ();
	  unsigned column = entry.column ();
	  bool is_stmt = entry.statement ();

#if 0
	  std::cout << std::hex << addr << std::dec
		    << "\t" << file
		    << "\t" << line
		    << "\t" << column
		    << "\t" << is_stmt
		    << "\t" << entry.end_sequence () << std::endl;
#endif

#define ADVANCE_OPCODE(WHAT, STEP, OPCODE)		\
	  {						\
	    __typeof (WHAT) const &what = WHAT;		\
	    __typeof (WHAT) const &reg_what = reg.WHAT;	\
	    unsigned step = STEP;			\
	    if (what != reg_what)			\
	      {						\
		__typeof (WHAT) delta = what - reg_what;\
		__typeof (WHAT) advance = delta / step;	\
		assert (advance * step == delta);	\
		standard_opcode (*this, OPCODE)		\
		  .arg (advance)			\
		  .write (appender);			\
	      }						\
	  }

#define SET_OPCODE(WHAT, OPCODE)			\
	  {						\
	    __typeof (WHAT) const &what = WHAT;		\
	    __typeof (WHAT) const &reg_what = reg.WHAT;	\
	    if (what != reg_what)			\
	      standard_opcode (*this, OPCODE)		\
		.arg (what)				\
		.write (appender);			\
	  }

#define SIMPLE_OPCODE(WHAT, OPCODE)		\
	  if (entry.WHAT ())			\
	    standard_opcode (*this, OPCODE)	\
	      .write (appender);

	  ADVANCE_OPCODE (addr, minimum_instruction_length, DW_LNS_advance_pc);
	  SET_OPCODE (file, DW_LNS_set_file);
	  ADVANCE_OPCODE (line, 1, DW_LNS_advance_line);
	  SET_OPCODE (column, DW_LNS_set_column);
	  SIMPLE_OPCODE (basic_block, DW_LNS_set_basic_block);
	  SIMPLE_OPCODE (prologue_end, DW_LNS_set_prologue_end);
	  SIMPLE_OPCODE (epilogue_begin, DW_LNS_set_epilogue_begin);

	  if (is_stmt != reg.is_stmt)
	    standard_opcode (*this, DW_LNS_negate_stmt)
	      .write (appender);

#undef SIMPLE_OPCODE
#undef SET_OPCODE
#undef ADVANCE_OPCODE

	  if (entry.end_sequence ())
	    {
	      extended_opcode (*this, DW_LNE_end_sequence)
		.write (appender);
	      reg.init ();
	    }
	  else
	    {
	      standard_opcode (*this, DW_LNS_copy)
		.write (appender);

	      reg.addr = addr;
	      reg.file = file;
	      reg.line = line;
	      reg.column = column;
	      reg.is_stmt = is_stmt;
	    }
	}

      table_length.finish ();

      if (!_m_line_offsets.insert (std::make_pair (&lt, offset_tab)).second)
	throw std::runtime_error ("duplicate line table address");
    }
}

void
dwarf_output::writer::output_debug_ranges (section_appender &appender)
{
  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);

  for (subr::value_set<dwarf_output::value::value_rangelistptr>::const_iterator
	 it = _m_col._m_ranges.begin (); it != _m_col._m_ranges.end (); ++it)
    {
      dwarf_output::range_list const &rl = it->second;
      if (!_m_range_offsets.insert (std::make_pair (&rl, appender.size ()))
	  .second)
	throw std::runtime_error ("duplicate range table address");

      for (dwarf_output::range_list::const_iterator range_it = rl.begin ();
	   range_it != rl.end (); ++range_it)
	{
	  write_form (inserter, DW_FORM_addr, range_it->first);
	  write_form (inserter, DW_FORM_addr, range_it->second);
	}

      // end of list entry
      write_form (inserter, DW_FORM_addr, 0);
      write_form (inserter, DW_FORM_addr, 0);
    }
}

void
dwarf_output::writer::output_debug_loc (section_appender &appender)
{
  typedef std::set <dwarf_output::location_attr const *> loc_set;
  loc_set locations;

  for (dwarf_output_collector::die_map::const_iterator it
	 = _m_col._m_unique.begin ();
       it != _m_col._m_unique.end (); ++it)
    {
      debug_info_entry const &die = it->first;
      for (debug_info_entry::attributes_type::const_iterator
	     at = die.attributes ().begin ();
	   at != die.attributes ().end (); ++at)
	{
	  attr_value const &value = at->second;
	  dwarf::value_space vs = value.what_space ();

	  if (vs == dwarf::VS_location)
	    {
	      dwarf_output::location_attr const &loc = value.location ();
	      if (loc.is_list ())
		locations.insert (&loc);
	    }
	}
    }

  std::back_insert_iterator <section_appender> inserter
    = std::back_inserter (appender);
  for (loc_set::const_iterator it = locations.begin ();
       it != locations.end (); ++it)
    {
      dwarf_output::location_attr const &loc = **it;
      if (!_m_loc_offsets.insert (std::make_pair (&loc, appender.size ()))
	  .second)
	throw std::runtime_error ("duplicate loc table address");

      for (dwarf_output::location_attr::const_iterator jt = loc.begin ();
	   jt != loc.end (); ++jt)
	{
	  write_form (inserter, DW_FORM_addr, jt->first.first);
	  write_form (inserter, DW_FORM_addr, jt->first.second);
	  write_form (inserter, DW_FORM_data2, jt->second.size ());
	  std::copy (jt->second.begin (), jt->second.end (), inserter);
	}

      // end of list entry
      write_form (inserter, DW_FORM_addr, 0);
      write_form (inserter, DW_FORM_addr, 0);
    }
}

void
dwarf_output::writer::apply_patches ()
{
  for (str_backpatch_vec::const_iterator it = _m_str_backpatch.begin ();
       it != _m_str_backpatch.end (); ++it)
    it->first.patch (ebl_strtaboffset (it->second));

  for (range_backpatch_vec::const_iterator it = _m_range_backpatch.begin ();
       it != _m_range_backpatch.end (); ++it)
    {
      range_offsets_map::const_iterator ot = _m_range_offsets.find (it->second);
      if (ot == _m_range_offsets.end ())
	// no point mentioning the key, since it's just a memory
	// address...
	throw std::runtime_error (".debug_ranges ref not found");
      it->first.patch (ot->second);
    }

  for (loc_backpatch_vec::const_iterator it = _m_loc_backpatch.begin ();
       it != _m_loc_backpatch.end (); ++it)
    {
      loc_offsets_map::const_iterator ot = _m_loc_offsets.find (it->second);
      if (ot == _m_loc_offsets.end ())
	// no point mentioning the key, since it's just a memory
	// address...
	throw std::runtime_error (".debug_loc ref not found");
      it->first.patch (ot->second);
    }
}
