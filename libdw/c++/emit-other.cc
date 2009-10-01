/* elfutils::dwarf_output routines for generation of various debug sections
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

      // xxx We emit everything with base address of 0.  Would be
      // better to figure out effective base address of referencing
      // CU, but this will do for the time being.
      // xxx When this is being fixed, duplicate selection has to take
      // base address into account.  So the set above will be set of
      // (location attr, base address) pairs.
      write_form (inserter, DW_FORM_addr, (uint64_t)-1);
      write_form (inserter, DW_FORM_addr, 0);

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
