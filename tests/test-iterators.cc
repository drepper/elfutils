/* Copyright (C) 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <cassert>
#include <stdexcept>

#include "../libdw/c++/libdw"
#include "../libdwfl/c++/libdwfl"
#include "../libdw/c++/libdwP.hh"
#include "../libdwfl/c++/libdwflP.hh"

int
main (int, char *argv[])
{
  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
    throw std::runtime_error (strerror (errno));

  const static Dwfl_Callbacks callbacks =
    {
      .find_elf = dwfl_build_id_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .section_address = dwfl_offline_section_address,
      .debuginfo_path = NULL,
    };

  Dwfl *dwfl = dwfl_begin (&callbacks);
  if (dwfl == NULL)
    throw_libdwfl ();

  dwfl_report_begin (dwfl);
  if (dwfl_report_offline (dwfl, argv[1], argv[1], fd) == NULL)
    throw_libdwfl ();
  dwfl_report_end (dwfl, NULL, NULL);

  for (elfutils::dwfl_module_iterator modit (dwfl);
       modit != elfutils::dwfl_module_iterator::end (); ++modit)
    {
      Dwarf_Off bias;
      Dwarf *dw = dwfl_module_getdwarf (&*modit, &bias);
      std::vector <std::pair <elfutils::unit_iterator, Dwarf_Die> > cudies;
      for (elfutils::unit_iterator it (dw); it != elfutils::unit_iterator::end (); ++it)
	cudies.push_back (std::make_pair (it, it->cudie));

      for (size_t i = 0; i < cudies.size (); ++i)
	{
	  elfutils::unit_iterator jt (dw, cudies[i].second);
	  std::cerr << std::hex << std::showbase
		    << dwarf_dieoffset (&jt->cudie) << std::endl;
	  for (size_t j = i; jt != elfutils::unit_iterator::end (); ++jt, ++j)
	    assert (jt == cudies[j].first);
	}

      assert (elfutils::die_tree_iterator (elfutils::unit_iterator::end ())
	      == elfutils::die_tree_iterator::end ());

      for (elfutils::die_tree_iterator it (dw);
	   it != elfutils::die_tree_iterator::end (); ++it)
	std::cerr << std::dec
		  << std::distance (elfutils::child_iterator (*it),
				    elfutils::child_iterator::end ()) << ' '
		  << std::distance (elfutils::attr_iterator (&*it),
				    elfutils::attr_iterator::end ())
		  << std::endl;
    }

  dwfl_end (dwfl);
}
