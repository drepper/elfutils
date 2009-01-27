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

  unsigned int expected = expected_value_space
    (dwarf_whatattr (thisattr ()), 0); // XXX need tag!

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
  os.setf(std::ios::hex, std::ios::basefield);
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
    case VS_dwarf_constant:
      return hex_string (constant ());

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
      return hex_string (reference ().offset (), "[", "]");

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
#define SIMPLE(type, name, form)				\
  type								\
  dwarf::attr_value::name () const				\
  {								\
    type result;						\
    xif (dwarf_form##form (thisattr (), &result) < 0);	\
    return result;						\
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
  xif (result == NULL);
  return result;
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
      xif (dwarf_formblock (thisattr (), &block) < 0);
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

  const uint8_t *const begin = reinterpret_cast<const uint8_t *> (block.data);
  const uint8_t *const end = begin + block.length;
  return const_vector<uint8_t> (begin, end);
}

// dwarf::source_file

const class dwarf::source_file
dwarf::attr_value::source_file () const
{
  switch (what_space ())
    {
    case VS_string:
    case VS_source_file:
      break;
    default:
      throw std::runtime_error ("XXX not a file name");
    }
  return source_file::source_file (_m_attr);
}

static bool
stringform (Dwarf_Attribute *attr)
{
  switch (dwarf_whatform (attr))
    {
    case DW_FORM_string:
    case DW_FORM_strp:
      return true;
    }
  return false;
}

static bool
get_files (Dwarf_Attribute *attr, Dwarf_Files **files, size_t *idx)
{
  Dwarf_Word result;
  CUDIE (cudie, attr->cu);
  if (dwarf_formudata (attr, &result) < 0
      || dwarf_getsrcfiles (&cudie, files, NULL) < 0)
    return true;
  *idx = result;
  return false;
}

Dwarf_Word
dwarf::source_file::mtime () const
{
  if (stringform (thisattr ()))
    return 0;

  Dwarf_Files *files;
  size_t idx;
  xif (get_files (thisattr (), &files, &idx));

  Dwarf_Word result;
  xif (dwarf_filesrc (files, idx, &result, NULL) == NULL);
  return result;
}

Dwarf_Word
dwarf::source_file::size () const
{
  if (stringform (thisattr ()))
    return 0;

  Dwarf_Files *files;
  size_t idx;
  xif (get_files (thisattr (), &files, &idx));

  Dwarf_Word result;
  xif (dwarf_filesrc (files, idx, NULL, &result) == NULL);
  return result;
}

const char *
dwarf::source_file::name () const
{
  if (stringform (thisattr ()))
    return dwarf_formstring (thisattr ());

  Dwarf_Files *files;
  size_t idx;
  xif (get_files (thisattr (), &files, &idx));

  const char *result = dwarf_filesrc (files, idx, NULL, NULL);
  xif (result == NULL);
  return result;
}

string
dwarf::source_file::to_string () const
{
  if (stringform (thisattr ()))
    return plain_string (dwarf_formstring (thisattr ()));

  Dwarf_Files *files;
  size_t idx;
  xif (get_files (thisattr (), &files, &idx));

  Dwarf_Word file_mtime;
  Dwarf_Word file_size;
  const char *result = dwarf_filesrc (files, idx, &file_mtime, &file_size);
  xif (result == NULL);

  if (likely (file_mtime == 0) && likely (file_size == 0))
    return plain_string (result);

  std::ostringstream os;
  os << "{\"" << result << "," << file_mtime << "," << file_size << "}";
  return os.str ();
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
dwarf::location_attr::singleton () const
{
  switch (dwarf_whatform (_m_attr.thisattr ()))
    {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      return true;
    }

  return false;
}

string
dwarf::location_attr::to_string () const
{
  if (singleton ())
    return "XXX";
  return hex_string (_m_attr.constant (), "#");
}

// dwarf::range_list

dwarf::range_list::const_iterator::const_iterator (Dwarf_Attribute *attr)
  : _m_base (-1), _m_cu (attr->cu), _m_offset (0)
{
  ++*this;
}

dwarf::range_list::const_iterator &
dwarf::range_list::const_iterator::operator++ ()
{
  const Elf_Data *d = _m_cu->dbg->sectiondata[IDX_debug_ranges];
  if (unlikely (d == NULL))
    throw std::runtime_error ("XXX no ranges");

  if (unlikely (_m_offset < 0) || unlikely ((size_t) _m_offset >= d->d_size))
    throw std::runtime_error ("XXX bad offset in ranges iterator");

  unsigned char *readp = (reinterpret_cast<unsigned char *> (d->d_buf)
			  + _m_offset);

  while (true)
    {
      if ((unsigned char *) d->d_buf + d->d_size - readp
	  < _m_cu->address_size * 2)
	throw std::runtime_error ("XXX bad ranges");

      if (_m_cu->address_size == 8)
	{
	  _m_begin = read_8ubyte_unaligned_inc (_m_cu->dbg, readp);
	  _m_end = read_8ubyte_unaligned_inc (_m_cu->dbg, readp);
	  if (_m_begin == (uint64_t) -1l) /* Base address entry.  */
	    {
	      _m_base = _m_end;
	      continue;
	    }
	}
      else
	{
	  _m_begin = read_4ubyte_unaligned_inc (_m_cu->dbg, readp);
	  _m_end = read_4ubyte_unaligned_inc (_m_cu->dbg, readp);
	  if (_m_begin == (uint32_t) -1) /* Base address entry.  */
	    {
	      _m_base = _m_end;
	      continue;
	    }
	}

      break;
    }

  if (_m_begin == 0 && _m_end == 0) /* End of list entry.  */
    _m_offset = 1;
  else
    {
      _m_offset = readp - reinterpret_cast<unsigned char *> (d->d_buf);

      if (_m_base == (Dwarf_Addr) -1)
	{
	  CUDIE (cudie, _m_cu);

	  /* Find the base address of the compilation unit.  It will
	     normally be specified by DW_AT_low_pc.  In DWARF-3 draft 4,
	     the base address could be overridden by DW_AT_entry_pc.  It's
	     been removed, but GCC emits DW_AT_entry_pc and not DW_AT_lowpc
	     for compilation units with discontinuous ranges.  */
	  Dwarf_Attribute attr_mem;
	  if (unlikely (dwarf_lowpc (&cudie, &_m_base) != 0)
	      && dwarf_formaddr (dwarf_attr (&cudie,
					     DW_AT_entry_pc,
					     &attr_mem),
				 &_m_base) != 0)
	    {
	      xif (true);		// XXX
	    }
	}
    }

  return *this;
}

template<typename container>
string
__libdw_ranges_to_string (const container &c)
{
  std::ostringstream os;
  os.setf(std::ios::hex, std::ios::basefield);

  os << "<";

  bool first = true;
  for (typename container::const_iterator i = c.begin (); i != c.end (); ++i)
    {
      typename container::value_type range = *i;
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
