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

#include <byteswap.h>
#include "dwarf_output"
#include "../../src/dwarfstrings.h"
using namespace elfutils;

namespace
{
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
dwarf_output::writer::write_uleb128 (Iterator it, uint64_t value)
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
dwarf_output::writer::write_sleb128 (Iterator it, int64_t value)
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

template <class Iterator>
void
dwarf_output::writer::write_u1 (Iterator it, uint8_t value)
{
  ::dw_write<1> (it, value, false);
}

template <class Iterator>
void
dwarf_output::writer::write_u2 (Iterator it, uint16_t value, bool big_endian)
{
  ::dw_write<2> (it, value, big_endian);
}

template <class Iterator>
void
dwarf_output::writer::write_u4 (Iterator it, uint32_t value, bool big_endian)
{
  ::dw_write<4> (it, value, big_endian);
}

template <class Iterator>
void
dwarf_output::writer::write_u8 (Iterator it, uint64_t value, bool big_endian)
{
  ::dw_write<8> (it, value, big_endian);
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


bool
dwarf_output::writer::source_file_is_string (int tag, int attr)
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

/* Return width of data stored with given form.  For block forms,
   return width of length field.  */
size_t
dwarf_output::writer::form_width (int form, bool addr_64, bool dwarf_64)
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

class dwarf_output::writer::length_field
{
  writer &_m_parent;
  elfutils::section_appender &_m_appender;
  const gap _m_length_gap;
  const size_t _m_cu_start;
  bool _m_finished;

  inline gap alloc_gap (elfutils::section_appender &appender) const
  {
    if (_m_parent._m_config.dwarf_64)
      dwarf_output::writer::write_u4 (appender.alloc (4), -1,
				      _m_parent._m_config.big_endian);
    gap g (_m_parent, appender, DW_FORM_ref_addr);
    g.set_base (appender.size ());
    return g;
  }

public:
  inline length_field (writer &parent,
		       elfutils::section_appender &appender)
    : _m_parent (parent),
      _m_appender (appender),
      _m_length_gap (alloc_gap (appender)),
      _m_cu_start (appender.size ()),
      _m_finished (false)
  {}

  inline void finish ()
  {
    assert (!_m_finished);
    _m_length_gap.patch (_m_appender.size ());
    _m_finished = true;
  }
};

template <class Iterator>
void
dwarf_output::writer::write_form (Iterator it, int form, uint64_t value)
{
  switch (form)
    {
    case DW_FORM_flag_present:
      return;

    case DW_FORM_flag:
      assert (value == 1 || value == 0);
    case DW_FORM_block1:
    case DW_FORM_data1:
    case DW_FORM_ref1:
      write_u1 (it, value);
      return;

    case DW_FORM_block2:
    case DW_FORM_data2:
    case DW_FORM_ref2:
      write_u2 (it, value, _m_config.big_endian);
      return;

    case DW_FORM_block4:
    case DW_FORM_data4:
    case DW_FORM_ref4:
      write_u4 (it, value, _m_config.big_endian);
      return;

    case DW_FORM_data8:
    case DW_FORM_ref8:
      write_u8 (it, value, _m_config.big_endian);
      return;

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
      write_uleb128 (it, value);
      return;

    case DW_FORM_sdata:
      write_sleb128 (it, value);
      return;
    }

  throw std::runtime_error (std::string ("Don't know how to write ")
			    + dwarf_form_string (form));
}

inline void
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

template <class Iterator>
void
dwarf_output::writer::write_var (Iterator it, unsigned width, uint64_t value)
{
  switch (width)
    {
    case 8:
      write_u8 (it, value, _m_config.big_endian);
      break;
    case 4:
      write_u4 (it, value, _m_config.big_endian);
      break;
    case 2:
      write_u2 (it, value, _m_config.big_endian);
      break;
    case 1:
      write_u1 (it, value);
    case 0:
      break;
    default:
      throw std::runtime_error ("Width has to be 0, 1, 2, 4 or 8.");
    }
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

void
dwarf_output::writer::write_version (section_appender &appender,
				     unsigned version)
{
  write_u2 (appender.alloc (2), version, _m_config.big_endian);
}
