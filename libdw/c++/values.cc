/* -*- C++ -*- interfaces for libdw.
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
#include <cassert>
#include "dwarf"

extern "C"
{
#include "libdwP.h"
}

using namespace elfutils;
using namespace std;

#include "dwarf-knowledge.cc"


// dwarf::attr_value disambiguation and dispatch.

/* For ambiguous the forms, we need to look up the expected
   value spaces for this attribute to disambiguate.
*/
dwarf::value_space
dwarf::attr_value::what_space () const
{
  unsigned int expected = expected_value_space (dwarf_whatattr (thisattr ()),
						_m_tag);
  unsigned int possible = 0;
  switch (dwarf_whatform (thisattr ()))
    {
    case DW_FORM_flag:
      return VS_flag;

    case DW_FORM_addr:
      return VS_address;

    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      /* Location expression or target constant.  */
      possible = VS(location) | VS(constant);
      if ((expected & possible) == possible)
	/* When both are expected, a block is a location expression.  */
	return VS_location;
      break;

    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_udata:
    case DW_FORM_sdata:
      /* Target constant, known DWARF constant, or *ptr.  */
      possible = (VS(dwarf_constant) | VS(constant)
		  | VS(source_file) | VS(source_line) | VS(source_column)
		  | VS(location) // loclistptr
		  | VS(lineptr) | VS(macptr) | VS(rangelistptr));
      if ((expected & possible) == (VS(constant) | VS(location)))
	/* When both are expected, a constant is not a loclistptr.  */
	return VS_constant;
      break;

    case DW_FORM_string:
    case DW_FORM_strp:
      /* Identifier, file name, or string.  */
      possible = VS(identifier) | VS(source_file) | VS(string);
      break;

    case DW_FORM_ref_addr:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      possible = VS(unit_reference) | VS(reference);
      break;

    default:
      throw std::runtime_error ("XXX bad form");
    }

  if (unlikely ((expected & possible) == 0))
    {
      if (expected == 0 && possible == (VS(unit_reference) | VS(reference)))
	// An unknown reference is a reference, not a unit_reference.
	return VS_reference;

      // Otherwise we don't know enough to treat it robustly.
      throw std::runtime_error ("XXX ambiguous form in unexpected attribute");
    }

  const int first = ffs (expected & possible) - 1;
  if (likely ((expected & possible) == (1U << first)))
    return static_cast<value_space> (first);

  throw std::runtime_error ("XXX ambiguous form");
}

static string
hex_string (Dwarf_Word value, const char *before = "", const char *after = "")
{
  std::ostringstream os;
  os.setf (std::ios::hex, std::ios::basefield);
  os.setf (std::ios::showbase);
  os << before << value << after;
  return os.str ();
}

static string
dec_string (Dwarf_Word value)
{
  std::ostringstream os;
  os << value;
  return os.str ();
}

static string
addr_string (Dwarf_Addr value)
{
  // XXX some hook for symbol resolver??
  return hex_string (value);
}

static inline string
plain_string (const char *filename)
{
  return string ("\"") + filename + "\"";
}

string
dwarf::attr_value::to_string () const
{
  switch (what_space ())
    {
    case VS_flag:
      return flag () ? "1" : "0";

    case VS_rangelistptr:
      return ranges ().to_string ();

    case VS_lineptr:	// XXX punt for now, treat as constant
    case VS_macptr:	// XXX punt for now, treat as constant
    case VS_constant:
      return hex_string (constant ());

    case VS_dwarf_constant:
      return dwarf_constant ().to_string ();

    case VS_source_line:
    case VS_source_column:
      return dec_string (constant ());

    case VS_identifier:
      return plain_string (identifier ());

    case VS_string:
      return plain_string (string ());

    case VS_address:
      return addr_string (address ());

    case VS_reference:
    case VS_unit_reference:
      return hex_string (reference ()->offset (), "[", "]");

    case VS_source_file:
      return source_file ().to_string ();

    case VS_location:
      return location ().to_string ();

    case VS_discr_list:
      break;			// XXX DW_AT_discr_list unimplemented
    }

  throw std::runtime_error ("XXX unsupported value space");
}

// A few cases are trivial.
#define SIMPLE(type, name, form)					\
  type									\
  dwarf::attr_value::name () const					\
  {									\
    type result;							\
    xif (thisattr (), dwarf_form##form (thisattr (), &result) < 0);	\
    return result;							\
  }

SIMPLE (bool, flag, flag)

// XXX check value_space is really constantish?? vs *ptr
SIMPLE (Dwarf_Word, constant, udata)
SIMPLE (Dwarf_Sword, signed_constant, sdata)

SIMPLE (Dwarf_Addr, address, addr)

const char *
dwarf::attr_value::string () const
{
  const char *result = dwarf_formstring (thisattr ());
  xif (thisattr(), result == NULL);
  return result;
}

bool
dwarf::attr_value::constant_is_integer () const
{
  switch (dwarf_whatform (thisattr ()))
    {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      return false;

    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_udata:
    case DW_FORM_sdata:
      return true;

    default:
      throw std::runtime_error ("XXX wrong form");
    }
}


const_vector<uint8_t>
dwarf::attr_value::constant_block () const
{
  Dwarf_Block block;

  switch (dwarf_whatform (thisattr ()))
    {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      xif (thisattr(), dwarf_formblock (thisattr (), &block) < 0);
      break;

    case DW_FORM_data1:
      block.length = 1;
      block.data = thisattr ()->valp;
      break;

    case DW_FORM_data2:
      block.length = 2;
      block.data = thisattr ()->valp;
      break;

    case DW_FORM_data4:
      block.length = 4;
      block.data = thisattr ()->valp;
      break;

    case DW_FORM_data8:
      block.length = 8;
      block.data = thisattr ()->valp;
      break;

    case DW_FORM_udata:
    case DW_FORM_sdata:
      // XXX ?
      if ((*(const uint8_t *) thisattr ()->valp & 0x80) == 0)
	{
	  block.length = 1;
	  block.data = thisattr ()->valp;
	  break;
	}

    default:
      throw std::runtime_error ("XXX wrong form");
    }

  return const_vector<uint8_t> (block);
}

const dwarf::macro_info_table
dwarf::attr_value::macro_info () const
{
  assert (dwarf_whatattr (thisattr ()) == DW_AT_macro_info);
  CUDIE (cudie, _m_attr.cu);
  debug_info_entry die;
  die._m_die = cudie;
  return macro_info_table (die);
}

// dwarf::range_list

dwarf::range_list::const_iterator::const_iterator (Dwarf_Attribute *attr,
						   ptrdiff_t offset)
  : _m_base (-1), _m_begin (0), _m_end (0), _m_cu (attr->cu), _m_offset (offset)
{
}

static bool
range_list_advance (int secndx,
		    Dwarf_CU *cu,
		    Dwarf_Addr &base,
		    Dwarf_Addr &begin,
		    Dwarf_Addr &end,
		    ptrdiff_t &offset,
		    unsigned char **valp)
{
  const Elf_Data *d = cu->dbg->sectiondata[secndx];
  if (unlikely (d == NULL))
    throw std::runtime_error ("XXX no ranges");

  if (unlikely (offset < 0) || unlikely ((size_t) offset >= d->d_size))
    throw std::runtime_error ("XXX bad offset in ranges iterator");

  unsigned char *readp = reinterpret_cast<unsigned char *> (d->d_buf) + offset;
  unsigned char *const readendp
    = reinterpret_cast<unsigned char *> (d->d_buf) + d->d_size;

  while (true)
    {
      if (readendp - readp < cu->address_size * 2)
	throw std::runtime_error ("XXX bad ranges");

      if (cu->address_size == 8)
	{
	  begin = read_8ubyte_unaligned_inc (cu->dbg, readp);
	  end = read_8ubyte_unaligned_inc (cu->dbg, readp);
	  if (begin == (uint64_t) -1l) /* Base address entry.  */
	    {
	      base = end;
	      continue;
	    }
	}
      else
	{
	  begin = read_4ubyte_unaligned_inc (cu->dbg, readp);
	  end = read_4ubyte_unaligned_inc (cu->dbg, readp);
	  if (begin == (uint32_t) -1) /* Base address entry.  */
	    {
	      base = end;
	      continue;
	    }
	}

      break;
    }

  if (begin == 0 && end == 0) /* End of list entry.  */
    offset = 1;
  else
    {
      if (valp)
	*valp = readp;
      offset = readp - reinterpret_cast<unsigned char *> (d->d_buf);

      if (base == (Dwarf_Addr) -1)
	{
	  CUDIE (cudie, cu);

	  /* Find the base address of the compilation unit.  It will
	     normally be specified by DW_AT_low_pc.  In DWARF-3 draft 4,
	     the base address could be overridden by DW_AT_entry_pc.  It's
	     been removed, but GCC emits DW_AT_entry_pc and not DW_AT_lowpc
	     for compilation units with discontinuous ranges.  */
	  Dwarf_Attribute attr_mem;
	  if (unlikely (dwarf_lowpc (&cudie, &base) != 0)
	      && dwarf_formaddr (dwarf_attr (&cudie,
					     DW_AT_entry_pc,
					     &attr_mem),
				 &base) != 0)
	    {
	      return true;	// XXX
	    }
	}
    }

  return false;
}

dwarf::range_list::const_iterator &
dwarf::range_list::const_iterator::operator++ ()
{
  xif (_m_cu, range_list_advance (IDX_debug_ranges, _m_cu, _m_base,
				  _m_begin, _m_end, _m_offset, NULL));
  return *this;
}


template<typename container>
string
__libdw_ranges_to_string (const container &c)
{
  std::ostringstream os;
  os.setf (std::ios::hex, std::ios::basefield);
  os.setf (std::ios::showbase);

  os << "<";

  bool first = true;
  for (typename container::const_iterator i = c.begin (); i != c.end (); ++i)
    {
      const typename container::value_type range = *i;
      if (!first)
	os << ",";
      os << range.first << "-" << range.second;
      first = false;
    }

  os << ">";

  return os.str ();
}

string
dwarf::range_list::to_string () const
{
  return __libdw_ranges_to_string (*this);
}

string
dwarf::ranges::to_string () const
{
  return __libdw_ranges_to_string (*this);
}

string
dwarf::arange_list::to_string () const
{
  return __libdw_ranges_to_string (*this);
}

dwarf::aranges_map
dwarf::aranges () const
{
  Dwarf_Aranges *these;
  xif (dwarf_getaranges (_m_dw, &these, NULL) < 0);

  if (these == NULL)
    return aranges_map ();

  aranges_map result;
  for (const Dwarf_Aranges_s::Dwarf_Arange_s *r = &these->info[0];
       r < &these->info[these->naranges];
       ++r)
    result[compile_unit (debug_info_entry (_m_dw, r->offset))].insert
      (arange_list::value_type (r->addr, r->addr + r->length));

  return result;
}

// dwarf::location_attr

const dwarf::location_attr
dwarf::attr_value::location () const
{
  if (what_space () != VS_location)
    throw std::runtime_error ("XXX not a location");

  return location_attr (*this);
}

bool
dwarf::location_attr::is_list () const
{
  switch (dwarf_whatform (_m_attr.thisattr ()))
    {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      return false;
    }

  return true;
}

dwarf::location_attr::const_iterator &
dwarf::location_attr::const_iterator::operator++ ()
{
  if (unlikely (_m_offset == 1))
    throw std::runtime_error ("incrementing end iterator");
  else if (_m_offset == 0)
    // Singleton, now at end.
    _m_offset = 1;
  else
    {
      // Advance to next list entry.
      xif (_m_attr._m_attr.thisattr (),
	   range_list_advance (IDX_debug_loc, _m_attr._m_attr._m_attr.cu,
			       _m_base, _m_begin, _m_end, _m_offset,
			       &_m_attr._m_attr._m_attr.valp));
      if (_m_offset > 1)
	{
	  _m_attr._m_attr._m_attr.form = DW_FORM_block2;
	  _m_offset += read_2ubyte_unaligned (_m_attr._m_attr._m_attr.cu->dbg,
					      _m_attr._m_attr._m_attr.valp);
	}
    }

  return *this;
}

string
dwarf::location_attr::to_string () const
{
  if (is_list ())
    return hex_string (_m_attr.constant (), "#");
  return "XXX-expr";
}
