/* Test program for CFI handling.
   Copyright (C) 2006, 2009 Red Hat, Inc.
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

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>

#include "../libdw/unwind.h"	/* XXX */

static void
print_detail (int result, const Dwarf_Op *ops, size_t nops)
{
  if (result < 0)
    printf ("indeterminate (%s)\n", dwarf_errmsg (-1));
  else if (nops == 0)
    printf ("%s\n", result == 0 ? "same_value" : "undefined");
  else
    {
      printf ("%s expression:", result == 0 ? "location" : "value");
      for (size_t i = 0; i < nops; ++i)
	{
	  printf (" %#x", ops[i].atom);
	  if (ops[i].number2 == 0)
	    {
	      if (ops[i].number != 0)
		printf ("(%" PRId64 ")", ops[i].number);
	    }
	  else
	    printf ("(%" PRId64 ",%" PRId64 ")",
		    ops[i].number, ops[i].number2);
	}
      puts ("");
    }
}

static int
print_register (void *arg,
		int regno,
		const char *setname,
		const char *prefix,
		const char *regname,
		int bits __attribute__ ((unused)),
		int type __attribute__ ((unused)))
{
  Dwarf_Frame *frame = arg;

  printf ("\t%s reg%u (%s%s): ", setname, regno, prefix, regname);

  Dwarf_Op ops_mem[2];
  Dwarf_Op *ops;
  size_t nops;
  int result = dwarf_frame_register (frame, regno, ops_mem, &ops, &nops);
  print_detail (result, ops, nops);

  return DWARF_CB_OK;
}

static void
handle_address (GElf_Addr pc, Dwfl *dwfl)
{
  Dwarf_Frame *frame;
  int result = dwfl_addrframe (dwfl, pc, &frame);
  if (result != 0)
    error (EXIT_FAILURE, 0, "dwfl_addrframe: %s", dwfl_errmsg (-1));

  Dwarf_Addr start = pc;
  Dwarf_Addr end = pc;
  bool signalp;
  int ra_regno = dwarf_frame_info (frame, &start, &end, &signalp);

  printf ("%#" PRIx64 " => [%#" PRIx64 ", %#" PRIx64 "):\n", pc, start, end);

  if (ra_regno < 0)
    printf ("\treturn address register unavailable (%s)\n",
	    dwarf_errmsg (0));
  else
    printf ("\treturn address in reg%u%s\n",
	    ra_regno, signalp ? " (signal frame)" : "");

  Dwarf_Op *cfa_ops;
  int cfa_nops = dwarf_frame_cfa (frame, &cfa_ops);
  if (cfa_nops < 0)
    error (EXIT_FAILURE, 0, "dwarf_frame_cfa: %s", dwarf_errmsg (-1));

  printf ("\tCFA ");
  print_detail (1, cfa_ops, cfa_nops);

  (void) dwfl_module_register_names (dwfl_addrmodule (dwfl, pc),
				     &print_register, frame);
}

int
main (int argc, char *argv[])
{
  int remaining;

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  Dwfl *dwfl = NULL;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);

  int result = 0;

  /* Now handle the addresses.  In case none are given on the command
     line, read from stdin.  */
  if (remaining == argc)
    {
      /* We use no threads here which can interfere with handling a stream.  */
      (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);

      char *buf = NULL;
      size_t len = 0;
      while (!feof_unlocked (stdin))
	{
	  if (getline (&buf, &len, stdin) < 0)
	    break;

	  char *endp;
	  uintmax_t addr = strtoumax (buf, &endp, 0);
	  if (endp != buf)
	    handle_address (addr, dwfl);
	  else
	    result = 1;
	}

      free (buf);
    }
  else
    {
      do
	{
	  char *endp;
	  uintmax_t addr = strtoumax (argv[remaining], &endp, 0);
	  if (endp != argv[remaining])
	    handle_address (addr, dwfl);
	  else
	    result = 1;
	}
      while (++remaining < argc);
    }

  dwfl_end (dwfl);

  return result;
}
