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
#include <tr1/functional>

#include "dwarf_output"
#include "emit-misc.hh"

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
	if (dwarf_output::writer::source_file_is_string (tag, attr))
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
	if (dwarf_output::writer::source_file_is_string (tag, attr))
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
	if (!dwarf_output::writer::source_file_is_string (tag, attr))
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
	    dwarf_output::writer::write_sleb128 (counter, value);
	  else
	    dwarf_output::writer::write_uleb128 (counter, value);
	  return count;
	}

      // xxx string logic should be extracted and treated using
      // different pass
      case DW_FORM_string:
	return value + 1; /* For strings, we yield string length plus
			     terminating zero.  */

      default:
	return dwarf_output::writer::form_width (form, addr_64, dwarf_64);
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
  dwarf_output::writer::write_uleb128 (inserter, inst.code);
  dwarf_output::writer::write_uleb128 (inserter, shape._m_tag);
  *inserter++ = shape._m_has_children ? DW_CHILDREN_yes : DW_CHILDREN_no;

  for (instance_type::forms_type::const_iterator it = inst.forms.begin ();
       it != inst.forms.end (); ++it)
    {
      // ULEB128 name & form
      dwarf_output::writer::write_uleb128 (inserter, it->first);
      dwarf_output::writer::write_uleb128 (inserter, it->second);
    }

  // 0 for name & form to terminate the abbreviation
  *inserter++ = 0;
  *inserter++ = 0;
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
