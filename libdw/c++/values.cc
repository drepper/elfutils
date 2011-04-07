/* -*- C++ -*- interfaces for libdw.
   Copyright (C) 2009-2011 Red Hat, Inc.
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
#include "dwarf_edit"
#include "dwarf_output"
#include "data-values.hh"

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
  const uint_fast16_t version = _m_attr.cu->version;
  unsigned int expected = expected_value_space (dwarf_whatattr (thisattr ()),
						_m_tag);
  unsigned int possible = 0;
  switch (dwarf_whatform (thisattr ()))
    {
    case DW_FORM_flag:
    case DW_FORM_flag_present:
      return VS_flag;

    case DW_FORM_addr:
      return VS_address;

    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      /* Location expression in DWARF 3, or target constant.  */
      possible = VS(constant);
      if (version >= 4)
	break;
      possible |= VS(location);
      if ((expected & possible) != possible)
	/* When both are expected, a block is a location expression.  */
	break;
      /* Fall through.  */

    case DW_FORM_exprloc:
      return VS_location;

    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_udata:
    case DW_FORM_sdata:
      /* Target constant, known DWARF constant.  */
      possible = (VS(dwarf_constant) | VS(constant)
		  | VS(source_file) | VS(source_line) | VS(source_column));
      break;

    case DW_FORM_data4:
    case DW_FORM_data8:
      // If a constant is not expected, these can be *ptr instead in DWARF 3.
      possible = (VS(dwarf_constant) | VS(constant)
		  | VS(source_file) | VS(source_line) | VS(source_column));
      if (version >= 4 || (expected & possible))
	break;

    case DW_FORM_sec_offset:
      possible = VS(location) | VS(lineptr) | VS(macptr) | VS(rangelistptr);
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
    case DW_FORM_ref_sig8:
      return VS_reference;

    default:
      throw std::runtime_error ("XXX bad form");
    }

  if (unlikely ((expected & possible) == 0))
    {
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
  os << std::hex << std::showbase << before << value << after;
  return os.str ();
}

static string
dec_string (Dwarf_Word value, const char *before = "", const char *after = "")
{
  std::ostringstream os;
  os << before << value << after;
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

static inline string
plain_string (const string &filename)
{
  return "\"" + filename + "\"";
}

template<class value_type>
static inline string
value_string (const value_type &value)
{
  switch (value.what_space ())
    {
    case dwarf::VS_flag:
      return value.flag () ? "1" : "0";

    case dwarf::VS_rangelistptr:
      return value.ranges ().to_string ();

    case dwarf::VS_lineptr:
      return value.line_info ().to_string ();

    case dwarf::VS_macptr:	// XXX punt for now, treat as constant
    case dwarf::VS_constant:
      if (value.constant_is_integer ())
	return hex_string (value.constant ());
      return dec_string (value.constant_block ().size (),
			 "{block of ", " bytes}");

    case dwarf::VS_dwarf_constant:
      return value.dwarf_constant ().to_string ();

    case dwarf::VS_source_line:
      return dec_string (value.source_line ());

    case dwarf::VS_source_column:
      return dec_string (value.source_column ());

    case dwarf::VS_identifier:
      return plain_string (value.identifier ());

    case dwarf::VS_string:
      return plain_string (value.string ());

    case dwarf::VS_address:
      return addr_string (value.address ());

    case dwarf::VS_reference:
      return hex_string (value.reference ()->offset (), "[", "]");

    case dwarf::VS_source_file:
      return value.source_file ().to_string ();

    case dwarf::VS_location:
      return value.location ().to_string ();

    case dwarf::VS_discr_list:
      break;			// XXX DW_AT_discr_list unimplemented
    }

  throw std::runtime_error ("XXX unsupported value space");
}

template<>
string
to_string<dwarf::attribute> (const dwarf::attribute &attr)
{
  return attribute_string (attr);
}

template<>
string
to_string<dwarf::attr_value> (const dwarf::attr_value &value)
{
  return value_string (value);
}

template<>
string
to_string<dwarf_edit::attr_value> (const dwarf_edit::attr_value &value)
{
  return value_string (value);
}

template<>
string
to_string<dwarf_output::attr_value> (const dwarf_output::attr_value &value)
{
  return value_string (value);
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
    case DW_FORM_exprloc:
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

namespace elfutils
{
  template<>
  std::string to_string (const dwarf::debug_info_entry &die)
  {
    return die_string (die);
  }
};

// dwarf::range_list

unsigned char *
dwarf::range_list::const_iterator::formptr (int secndx, Dwarf_Attribute *attr)
{
  unsigned char *readptr = __libdw_formptr (attr, secndx,
					    DWARF_E_NO_DEBUG_RANGES,
					    NULL, NULL);
  xif (attr, readptr == NULL);
  return readptr;
}

dwarf::range_list::const_iterator
dwarf::range_list::begin () const
{
  const_iterator it (IDX_debug_ranges, _m_attr.thisattr (), 0);
  return ++it;
}

dwarf::range_list::const_iterator::const_iterator (int secndx,
						   Dwarf_Attribute *attr,
						   unsigned char *readptr)
  : _m_base (-1), _m_begin (0), _m_end (0), _m_cu (attr->cu)
  , _m_readptr (readptr)
{
  if (_m_readptr == NULL)
    {
      _m_readptr = formptr (secndx, attr);
      xif (attr, _m_readptr == NULL);
    }
}

static bool
range_list_advance (int secndx,
		    Dwarf_CU *cu,
		    Dwarf_Addr &base,
		    Dwarf_Addr &begin,
		    Dwarf_Addr &end,
		    unsigned char *&readp,
		    unsigned char **valp)
{
  const Elf_Data *d = cu->dbg->sectiondata[secndx];
  if (unlikely (d == NULL))
    throw std::runtime_error ("XXX no ranges");

  if (unlikely (readp >= (unsigned char *)d->d_buf + d->d_size))
    throw std::runtime_error ("XXX bad readptr in ranges iterator");

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
    readp = (unsigned char *)-1;
  else
    {
      if (valp)
	*valp = readp;

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
				  _m_begin, _m_end, _m_readptr, NULL));
  return *this;
}


template<typename container>
string
__libdw_ranges_to_string (const container &c)
{
  std::ostringstream os;

  os << "<" << std::hex << std::showbase;

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
  if (_m_attr.thisattr ()->cu->version >= 4)
    return dwarf_whatform (_m_attr.thisattr ()) == DW_FORM_sec_offset;

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

inline void
dwarf::location_attr::const_iterator::advance ()
{
  xif (_m_cu, range_list_advance (IDX_debug_loc, _m_cu,
				  _m_base, _m_begin, _m_end, _m_readptr,
				  &_m_block.data));
  // Special values are (unsigned char *){-1, 0, 1}.
  if (((uintptr_t)_m_readptr + 1) > 2)
    _m_readptr += 2 + (_m_block.length
		       = read_2ubyte_unaligned_inc (_m_cu->dbg, _m_block.data));
  else
    // End iterator.
    _m_block = Dwarf_Block ();
}

dwarf::location_attr::const_iterator
dwarf::location_attr::begin () const
{
  const_iterator i (_m_attr.thisattr ());
  if (is_list ())
    {
      i._m_readptr = const_iterator::formptr (IDX_debug_loc,
					      _m_attr.thisattr ());
      xif (_m_attr.thisattr (), i._m_readptr == NULL);
      i.advance ();
    }
  else
    {
      xif (_m_attr.thisattr (),
	   dwarf_formblock (_m_attr.thisattr (), &i._m_block) < 0);
      i._m_base = 0;
      i._m_end = -1;
      i._m_readptr = NULL;
    }

  return i;
}

dwarf::location_attr::const_iterator &
dwarf::location_attr::const_iterator::operator++ ()
{
  if (unlikely (_m_readptr == (unsigned char *)-1))
    throw std::runtime_error ("incrementing end iterator");

  if (_m_readptr == NULL)
    {
      // Singleton, now at end.
      _m_readptr = (unsigned char *)-1;
      _m_block.data = NULL;
      _m_block.length = 0;
    }
  else
    // Advance to next list entry.
    advance ();

  return *this;
}

template<typename locattr>
static string
locattr_string (const locattr *loc)
{
  return (loc->is_list () ? dec_string (loc->size (), "{loclist ", " entries}")
	  : "{locexpr}");
}

string
dwarf::location_attr::to_string () const
{
  return locattr_string (this);
}

string
dwarf_data::location_attr::to_string () const
{
  return locattr_string (this);
}

// dwarf::line_info_table

template<typename line_info_table>
static inline std::string
line_info_string (const line_info_table *table)
{
  return ("[" + table->lines ().to_string () + "]");
}

std::string
dwarf::line_info_table::to_string () const
{
  return line_info_string (this);
}

namespace elfutils
{
  template<>
  std::string
  dwarf_edit::line_info_table::to_string () const
  {
    return line_info_string (this);
  }

};

// dwarf::line_table

std::string
dwarf::line_table::to_string () const
{
  return dec_string (_m_lines->nlines, "{", " line entries}");
}

namespace elfutils
{
  template<>
  std::string
  dwarf_edit::line_table::to_string () const
  {
    return dec_string (size (), "{", " line entries}");
  }
};

::Dwarf_Off
dwarf::debug_info_entry::cost () const
{
  Dwarf_Die next;
  int result = dwarf_siblingof (thisdie (), &next);
  xif (result < 0);
  if (result == 0)
    return (const char *) next.addr - (const char *) _m_die.addr;
  if (next.addr != NULL)
    return (const char *) next.addr - (const char *) _m_die.addr + 1;
  return _m_die.cu->end - dwarf_dieoffset (thisdie ());
}
