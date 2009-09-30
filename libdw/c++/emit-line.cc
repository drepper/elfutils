/* elfutils::dwarf_output line number program generation.
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
#include "../../src/dwarf-opcodes.h"
#include "emit-misc.hh"

using namespace elfutils;
#define __unused __attribute__ ((unused))

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
    dwarf_output::writer::write_uleb128 (std::back_inserter (appender),
					 _m_buf.size () + 1);
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
	    && !dwarf_output::writer::source_file_is_string (die.tag (),
							     at->first))
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
      write_version (appender, 3);
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
	     = dirs.begin () + 1; dir_it < dirs.end (); ++dir_it)
	// +1 above skips the directory that libdw reports for
	// DW_AT_comp_dir attribute.
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
	  source_file const &sf = sfit->first;

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
	  write_uleb128 (inserter, dir_index);
	  write_uleb128 (inserter, sf.mtime ());
	  write_uleb128 (inserter, sf.size ());
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
