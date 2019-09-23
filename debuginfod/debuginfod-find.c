/* Command-line frontend for retrieving ELF / DWARF / source files
   from the debuginfod.
   Copyright (C) 2019 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "printversion.h"
#include "debuginfod.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Short description of program.  */
static const char doc[] = N_("Request debuginfo-related content "
                             "from debuginfods listed in $" DEBUGINFOD_URLS_ENV_VAR ".");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("debuginfo BUILDID\n"
                                  "executable BUILDID\n"
                                  "source BUILDID /FILENAME");

/* Data structure to communicate with argp functions.  */
static struct argp argp =
  {
   NULL, NULL, args_doc, doc, NULL, NULL, NULL
  };



int
main(int argc, char** argv)
{
  int remaining;
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER|ARGP_NO_ARGS, &remaining, NULL);

  if (argc < 2 || remaining+1 == argc) /* no arguments or at least two non-option words */
    {
      argp_help (&argp, stderr, ARGP_HELP_USAGE, argv[0]);
      return 1;
    }
  
  int rc;
  char *cache_name;

  /* Check whether FILETYPE is valid and call the appropriate
     debuginfod_find_* function. If FILETYPE is "source"
     then ensure a FILENAME was also supplied as an argument.  */
  if (strcmp(argv[remaining], "debuginfo") == 0)
    rc = debuginfod_find_debuginfo((unsigned char *)argv[remaining+1], 0, &cache_name);
  else if (strcmp(argv[remaining], "executable") == 0)
    rc = debuginfod_find_executable((unsigned char *)argv[remaining+1], 0, &cache_name);
  else if (strcmp(argv[remaining], "source") == 0)
    {
      if (remaining+2 == argc || argv[3][0] != '/')
        {
          fprintf(stderr, "If FILETYPE is \"source\" then absolute /FILENAME must be given\n");
          return 1;
        }
      rc = debuginfod_find_source((unsigned char *)argv[remaining+1], 0,
                                 argv[remaining+2], &cache_name);
    }
  else
    {
      argp_help (&argp, stderr, ARGP_HELP_USAGE, argv[0]);
      return 1;
    }

  if (rc < 0)
    {
      fprintf(stderr, "Server query failed: %s\n", strerror(-rc));
      return 1;
    }

  printf("%s\n", cache_name);
  return 0;
}
