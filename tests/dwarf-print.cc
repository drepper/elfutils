/* Test program for elfutils::dwarf basics.
   Copyright (C) 2009 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <libintl.h>

#include "c++/dwarf"

using namespace elfutils;
using namespace std;

#include "print-die.hh"

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

int
main (int argc, char *argv[])
{
  unsigned int depth;
  print_die_main (argc, argv, depth);

  for (int i = 1; i < argc; ++i)
    print_file (argv[i], dwarf (open_file (argv[i])), depth);

  return 0;
}
