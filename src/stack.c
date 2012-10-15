/* Unwinding of frames like gstack/pstack.
   Copyright (C) 2012 Red Hat, Inc.
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
#include <argp.h>
#include <error.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include ELFUTILS_HEADER(dwfl)

/* libdwfl/argp-std.c */
#define OPT_COREFILE    0x101

static void
report_pid (Dwfl *dwfl, pid_t pid)
{
  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));
}

static void
dump (Dwfl *dwfl, pid_t pid, const char *corefile)
{
  if (pid)
    report_pid (dwfl, pid);
  Dwfl_Frame_State *state;
  if (pid)
    state = dwfl_frame_state_pid (dwfl, pid);
  else if (corefile)
    state = dwfl_frame_state_core (dwfl, corefile);
  else
    abort ();
  if (state == NULL)
    error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
  do
    {
      Dwfl_Frame_State *thread = state;
      pid_t tid = dwfl_frame_tid_get (thread);
      printf ("TID %ld:\n", (long) tid);
      unsigned frameno;
      for (frameno = 0; state; frameno++)
	{
	  Dwarf_Addr pc;
	  bool minusone;
	  if (! dwfl_frame_state_pc (state, &pc, &minusone))
	    {
	      fprintf (stderr, "%s\n", dwfl_errmsg (-1));
	      break;
	    }
	  Dwarf_Addr pc_adjusted = pc - (minusone ? 1 : 0);

	  /* Get PC->SYMNAME.  */
	  Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
	  const char *symname = NULL;
	  if (mod)
	    symname = dwfl_module_addrname (mod, pc_adjusted);

	  printf ("#%2u %#" PRIx64 "%4s\t%s\n", frameno, (uint64_t) pc,
		  minusone ? "- 1" : "", symname);
	  if (! dwfl_frame_unwind (&state))
	    {
	      fprintf (stderr, "%s\n", dwfl_errmsg (-1));
	      break;
	    }
	}
      state = dwfl_frame_thread_next (thread);
    }
  while (state);

  dwfl_end (dwfl);
}

static argp_parser_t parse_opt_orig;
static pid_t pid;
static const char *corefile;

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'p':
      pid = atoi (arg);
      break;
    case OPT_COREFILE:
      corefile = arg;
      break;
    }
  return parse_opt_orig (key, arg, state);
}

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  struct argp argp = *dwfl_standard_argp ();
  parse_opt_orig = argp.parser;
  argp.parser = parse_opt;
  int remaining;
  Dwfl *dwfl = NULL;
  argp_parse (&argp, argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);
  assert (remaining == argc);

  if (pid && !corefile)
    dump (dwfl, pid, NULL);
  else if (corefile && !pid)
    dump (dwfl, 0, corefile);
  else
    error (2, 0, "eu-stack [--debuginfo-path=<path>] {-p <process id>|"
                 "--core=<file> [--executable=<file>]|--help}");

  return 0;
}
