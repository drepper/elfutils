/* elfutils::dwarf_output generation of .debug_info
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
    dwarf_output::writer::write_uleb128 (inserter, instance.code);

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
	    if (dwarf_output::writer::source_file_is_string (tag, attr))
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
      write_version (appender, 3);

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
