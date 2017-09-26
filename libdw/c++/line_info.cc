/* -*- C++ -*- interfaces for libdw.
   Copyright (C) 2009-2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <cassert>
#include "dwarf"

extern "C"
{
#include "libdwP.h"
}

using namespace elfutils;
using namespace std;


// dwarf::line_info_table

const dwarf::line_info_table
dwarf::attr_value::line_info () const
{
  assert (dwarf_whatattr (thisattr ()) == DW_AT_stmt_list);

  CUDIE (cudie, _m_attr.cu);
  Dwarf_Lines *lines;
  size_t n;
  xif (thisattr (), dwarf_getsrclines (&cudie, &lines, &n) < 0);

  return line_info_table (_m_attr.cu->files);
}

const dwarf::line_table
dwarf::line_info_table::lines () const
{
  return line_table (_m_files->cu->lines);
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
  return dwarf::source_file (_m_attr);
}

static bool
stringform (Dwarf_Attribute *attr)
{
  if (attr->valp != NULL)
    switch (dwarf_whatform (attr))
      {
      case DW_FORM_string:
      case DW_FORM_strp:
	return true;
      }
  return false;
}

/* Returns true if the attribute represents a valid zero udata.
   This represents "no-file".  */
static bool
zero_formudata (Dwarf_Attribute *attr)
{
  Dwarf_Word zero;
  return dwarf_formudata (attr, &zero) == 0 && zero == 0;
}

/* Mock up a dummy attribute with a special kludge that get_files groks.
   We use these for source_file objects consed directly from an index
   rather than from a real attribute.  */
static inline const Dwarf_Attribute
dummy_source_file (Dwarf_CU *cu, unsigned int idx)
{
  const Dwarf_Attribute dummy = { idx, DW_FORM_indirect, NULL, cu };
  return dummy;
}

static bool
get_files (const Dwarf_Attribute *attr, Dwarf_Files **files, Dwarf_Word *idx)
{
  if (attr->valp == NULL)
    {
      // Dummy hack created by dummy_source_file, above.
      assert (attr->form == DW_FORM_indirect);
      *files = attr->cu->files;
      *idx = attr->code;
      return false;
    }

  CUDIE (cudie, attr->cu);
  return (dwarf_formudata (const_cast<Dwarf_Attribute *> (attr), idx) < 0
	  || dwarf_getsrcfiles (&cudie, files, NULL) < 0);
}

Dwarf_Word
dwarf::source_file::mtime () const
{
  if (stringform (thisattr ()) || zero_formudata (thisattr ()))
    return 0;

  Dwarf_Files *files;
  Dwarf_Word idx;
  xif (thisattr (), get_files (thisattr (), &files, &idx));

  Dwarf_Word result;
  xif (thisattr (), dwarf_filesrc (files, idx, &result, NULL) == NULL);
  return result;
}

Dwarf_Word
dwarf::source_file::size () const
{
  if (stringform (thisattr ()) || zero_formudata (thisattr ()))
    return 0;

  Dwarf_Files *files;
  Dwarf_Word idx;
  xif (thisattr (), get_files (thisattr (), &files, &idx));

  Dwarf_Word result;
  xif (thisattr (), dwarf_filesrc (files, idx, NULL, &result) == NULL);
  return result;
}

static const char *no_file = "";

const char *
dwarf::source_file::name () const
{
  const char *result;
  if (stringform (thisattr ()))
    result = dwarf_formstring (thisattr ());
  else if (zero_formudata (thisattr ()))
    result = no_file;
  else
    {
      Dwarf_Files *files;
      Dwarf_Word idx;
      xif (thisattr (), get_files (thisattr (), &files, &idx));
     result = dwarf_filesrc (files, idx, NULL, NULL);
    }
  xif (thisattr (), result == NULL);
  return result;
}

static inline string
plain_string (const char *filename)
{
  string result ("\"");
  result += filename;
  result += "\"";
  return result;
}

string
dwarf::source_file::to_string () const
{
  if (stringform (thisattr ()))
    {
      const char *result = dwarf_formstring (thisattr ());
      xif (thisattr (), result == NULL);
      return plain_string (result);
    }

  if (zero_formudata (thisattr ()))
    return plain_string (no_file);

  Dwarf_Files *files;
  Dwarf_Word idx;
  xif (thisattr (), get_files (thisattr (), &files, &idx));

  Dwarf_Word file_mtime;
  Dwarf_Word file_size;
  const char *result = dwarf_filesrc (files, idx, &file_mtime, &file_size);
  xif (thisattr (), result == NULL);

  if (likely (file_mtime == 0) && likely (file_size == 0))
    return plain_string (result);

  std::ostringstream os;
  os << "{\"" << result << "," << file_mtime << "," << file_size << "}";
  return os.str ();
}

// dwarf::file_table

size_t
dwarf::file_table::size () const
{
  return _m_files->nfiles;
}

const dwarf::source_file
dwarf::file_table::at (size_t idx) const
{
  if (unlikely (idx >= _m_files->nfiles))
    throw std::out_of_range ("XXX fileidx");

  return dwarf::source_file (dummy_source_file (_m_files->cu, idx));
}

dwarf::file_table::const_iterator
dwarf::file_table::find (const source_file &src) const
{
  if (src._m_attr.cu->files == _m_files)
    {
      // Same table, just cons an iterator using its index.
      Dwarf_Files *files;
      Dwarf_Word idx;
      xif (files->cu, get_files (&src._m_attr, &files, &idx));
      return const_iterator (*this, idx);
    }

  // Not from this table, just match on file name.
  return find (src.name ());
}

// dwarf::line_table

size_t
dwarf::line_table::size () const
{
  return _m_lines->nlines;
}

const dwarf::line_entry
dwarf::line_table::at (size_t idx) const
{
  if (unlikely (idx >= _m_lines->nlines))
    throw std::out_of_range ("XXX line table index");

  return line_entry (reinterpret_cast<Dwarf_Line *> (&_m_lines->info[idx]));
}

dwarf::line_table::const_iterator
dwarf::line_table::find (Dwarf_Addr address) const
{
  size_t idx = _m_lines->nlines; // end ()
  if (likely (idx > 0))
    {
      CUDIE (cudie, _m_lines->info[0].files->cu);
      Dwarf_Line *line = dwarf_getsrc_die (&cudie, address);
      if (line != NULL)
	idx = line - &_m_lines->info[0];
    }
  return const_iterator (*this, idx);
}

// dwarf::line_entry

const dwarf::source_file
dwarf::line_entry::file () const
{
  return dwarf::source_file (dummy_source_file (_m_line->files->cu,
						_m_line->file));
}

#define LINEFIELD(type, method, field)		\
  type						\
  dwarf::line_entry::method () const		\
  {						\
    return _m_line->field;			\
  }

LINEFIELD (Dwarf_Addr, address, addr) // XXX dwfl?
LINEFIELD (unsigned int, line, line)
LINEFIELD (unsigned int, column, column)
LINEFIELD (bool, statement, is_stmt)
LINEFIELD (bool, basic_block, basic_block)
LINEFIELD (bool, end_sequence, end_sequence)
LINEFIELD (bool, prologue_end, prologue_end)
LINEFIELD (bool, epilogue_begin, epilogue_begin)

#undef	LINEFIELD

bool
dwarf::line_entry::operator== (const dwarf::line_entry &other) const
{
  Dwarf_Line *const a = _m_line;
  Dwarf_Line *const b = other._m_line;

  if (a == b)
    return true;

  if (a->addr != b->addr
      || a->line != b->line
      || a->column != b->column
      || a->is_stmt != b->is_stmt
      || a->basic_block != b->basic_block
      || a->end_sequence != b->end_sequence
      || a->prologue_end != b->prologue_end
      || a->epilogue_begin != b->epilogue_begin)
    return false;

  // Everything else matches, now have to try the file.
  if (a->files == b->files)
    // Same table, just compare indices.
    return a->file == b->file;

  Dwarf_Word atime;
  Dwarf_Word asize;
  const char *aname = dwarf_linesrc (a, &atime, &asize);
  xif (a->files->cu, aname == NULL);
  Dwarf_Word btime;
  Dwarf_Word bsize;
  const char *bname = dwarf_linesrc (b, &btime, &bsize);
  xif (b->files->cu, bname == NULL);

  /* The mtime and size only count when encoded as nonzero.
     If either side is zero, we don't consider the field.  */

  if (atime != btime && atime != 0 && btime != 0)
    return false;

  if (asize != bsize && asize != 0 && bsize != 0)
    return false;

  return !strcmp (aname, bname);
}

// dwarf::compile_unit convenience functions.

const dwarf::line_info_table
dwarf::compile_unit::line_info () const
{
  Dwarf_Lines *l;
  size_t n;
  xif (dwarf_getsrclines (thisdie (), &l, &n) < 0);

  return line_info_table (thisdie ()->cu->files);
}
