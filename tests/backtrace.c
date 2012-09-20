/* Test program for libdwfl file decriptors leakage.
   Copyright (C) 2007, 2008 Red Hat, Inc.
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

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <dwarf.h>
#include <sys/resource.h>
#include "../libdw/libdwP.h" /* for DWARF_E_RA_UNDEFINED ? */
#include ELFUTILS_HEADER(dwfl)


static void
dump (pid_t pid)
{
  static char *debuginfo_path;
  static const Dwfl_Callbacks proc_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,

      .find_elf = dwfl_linux_proc_find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&proc_callbacks);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));

  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));

  Dwarf_Frame_State *state = dwfl_frame_state (dwfl);
  if (state == NULL)
    error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
  while (state)
    {
      Dwarf_Addr pc = dwarf_frame_state_pc (state);
      int dw_errno = dwarf_errno ();
      if (dw_errno == DWARF_E_RA_UNDEFINED)
	break;
      if (dw_errno != DWARF_E_NOERROR)
	error (2, 0, "dwarf_frame_state_pc: %s", dwarf_errmsg (dw_errno));
      printf ("%p\n", (void *) pc);
      state = dwfl_frame_unwind (state);
      if (state == NULL)
	error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
    }

  dwfl_end (dwfl);
}

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  if (argc <= 1)
    {
      pid_t pid = fork ();
      switch (pid)
      {
	case -1:
	  abort ();
	case 0:
	  sleep (60);
	  abort ();
	default:
	  break;
      }
      dump (pid);
    }
  else while (*++argv)
    {
      int pid = atoi (*argv);
      if (argc > 2)
	printf ("PID %d\n", pid);
      dump (pid);
    }

  return 0;
}
