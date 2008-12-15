/* Copyright (C) 2007,2008 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2007.

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

#include ELFUTILS_HEADER(dwfl)
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>

static int my_find_elf (Dwfl_Module *, void **, const char *, Dwarf_Addr,
			char **, Elf **);

static const Dwfl_Callbacks my_callbacks =
{
  .find_elf = my_find_elf,
};

static int
my_find_elf (Dwfl_Module * mod __attribute__ ((unused)),
	     void ** userdata __attribute__ ((unused)),
	     const char * module_name __attribute__ ((unused)),
	     Dwarf_Addr addr __attribute__ ((unused)),
	     char ** file_name __attribute__ ((unused)),
	     Elf ** elf __attribute__ ((unused)))
{
  return 0;
}

int
main(int argc, char ** argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Order is significant.  If in doubt, check that libdwfl's
   * resolve_symbol still gets hit:
   *  $ ftrace -sym=resolve_symbol ./relocate ~/testfile9
   * should give you roughly:
   *  14395.14395 attached /home/ant/elfutils-mtn/build/tests/relocate
   *  14395.14395 call libdw.so.1->resolve_symbol(0x96e6758, 0x96e6800, 0xbfa4b474, 0x0, 0xbfa4b470, 0xbfa4b4ac) = 12
   *  14395.14395 call libdw.so.1->resolve_symbol(0x96e6758, 0x96e6800, 0xbfa4b474, 0x0, 0xbfa4b470, 0xbfa4b4ac) = 12
   *  14395.14395 call libdw.so.1->resolve_symbol(0x96e6758, 0x96e6800, 0xbfa4b474, 0x0, 0xbfa4b470, 0xbfa4b4ac) = 12
   *  14395.14395 exited with status 0
   *
   * Granted this test is very fragile, and depends a lot on knowledge
   * of libdwfl internals.
   */
  if (argc < 2)
    exit(77);
  char const *name = argv[1];
  char const *name2 = argv[0];

  Dwfl *dwfl = dwfl_begin (&my_callbacks);
  dwfl_report_begin (dwfl);
  Dwfl_Module *module = dwfl_report_offline (dwfl, name, name, -1);
  if (module == NULL)
    {
      fprintf (stderr, "DWFL error 1: %s\n", dwfl_errmsg (dwfl_errno ()));
      exit (1);
    }
  Dwfl_Module *module2 = dwfl_report_offline (dwfl, name2, name2, -1);
  if (module2 == NULL)
    {
      fprintf (stderr, "DWFL error 2: %s\n", dwfl_errmsg (dwfl_errno ()));
      exit (1);
    }
  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    {
      fprintf (stderr, "DWFL error 3: %s\n", dwfl_errmsg (dwfl_errno ()));
      exit (1);
    }
  Dwarf_Addr base;
  Elf * elf = dwfl_module_getelf (module, &base);
  if (elf == NULL)
    {
      fprintf (stderr, "DWFL error 4: %s\n", dwfl_errmsg (dwfl_errno ()));
      exit (1);
    }

  dwfl_end (dwfl);
  exit(0);
}
