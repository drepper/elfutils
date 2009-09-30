/* elfutils::dwarf_output implementation of writer itself.
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
#include "emit-misc.hh"
using namespace elfutils;

dwarf_output::writer::gap::gap (writer &parent)
  : _m_parent (parent),
    _m_ptr (NULL)
{}

dwarf_output::writer::gap::gap (writer &parent, section_appender &appender,
				int form, uint64_t base)
  : _m_parent (parent),
    _m_ptr (appender.alloc (writer::form_width (form, parent._m_config.addr_64,
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


dwarf_output::writer::configuration::configuration (bool a_big_endian,
						    bool a_addr_64,
						    bool a_dwarf_64)
  : big_endian (a_big_endian),
    addr_64 (a_addr_64),
    dwarf_64 (a_dwarf_64)
{}


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
