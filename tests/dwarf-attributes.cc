/* Test program for elfutils::dwarf basics.
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

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <clocale>
#include <libintl.h>
#include <ostream>
#include <iomanip>
#include <known-dwarf.h>

#include "c++/dwarf"

using namespace elfutils;

const char *
dwarf_attr_string (unsigned int attrnum)
{
  switch (attrnum)
    {
#define ONE_KNOWN_DW_AT(NAME, TAG)	\
      case TAG: return #NAME;
      ALL_KNOWN_DW_AT
#undef ONE_KNOWN_DW_AT
    default:
	static char buf[40];
 	if (attrnum < DW_AT_lo_user)
 	  snprintf (buf, sizeof buf, "unknown attribute %hx",
 		    attrnum);
 	else
 	  snprintf (buf, sizeof buf, "unknown user attribute %hx",
 		    attrnum);
	return buf;
    }
}

static Dwarf *
open_file (const char *fname)
{
  int fd = open (fname, O_RDONLY);
  if (unlikely (fd == -1))
    error (2, errno, gettext ("cannot open '%s'"), fname);
  Dwarf *dw = dwarf_begin (fd, DWARF_C_READ);
  if (dw == NULL)
    {
      error (2, 0,
	     gettext ("cannot create DWARF descriptor for '%s': %s"),
	     fname, dwarf_errmsg (-1));
    }
  return dw;
}

void
process_file (char const *file)
{
  dwarf dw (open_file (file));

  std::cout << file << ":\n";

  elfutils::dwarf::compile_unit const &cu = *dw.compile_units ().begin ();
  for (elfutils::dwarf::debug_info_entry::attributes::const_iterator
	 it = cu.attributes ().begin (); it != cu.attributes ().end (); ++it)
    {
      elfutils::dwarf::attribute at = *it;
      std::cout << (*it).first << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  for (int i = 1; i < argc; ++i)
    process_file (argv[i]);

  return 0;
}
