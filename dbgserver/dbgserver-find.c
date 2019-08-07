/* Command-line frontend for retrieving ELF / DWARF / source files
   from the dbgserver.
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

#include "dbgserver-client.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>


/*
   Command-line frontend for dbgserver.

   Query dbgserver for the file with the FILETYPE and BUILDID given as
   command-line arguments. FILETYPE must be one of "debuginfo", "executable"
   or "source-file". BUILDID must be a hex string with even length. If
   FILETYPE is "source-file" then a FILENAME must also be supplied as a
   command-line argument, otherwise FILENAME is ignored and may be omitted.

   If the file is successfully retrieved from the server, print the file's
   path to stdout, otherwise print an error message describing the failure.
*/
int
main(int argc, char** argv)
{
  if (argc < 3 || argc > 4)
    {
      fprintf(stderr, "usage: %s FILETYPE BUILDID FILENAME\n", argv[0]);
      return 1;
    }

  int rc;
  char *cache_name;

  /* Check whether FILETYPE is valid and call the appropriate
     dbgserver_find_* function. If FILETYPE is "source-file"
     then ensure a FILENAME was also supplied as an argument.  */
  if (strcmp(argv[1], "debuginfo") == 0)
    rc = dbgserver_find_debuginfo((unsigned char *)argv[2], 0, &cache_name);
  else if (strcmp(argv[1], "executable") == 0)
    rc = dbgserver_find_executable((unsigned char *)argv[2], 0, &cache_name);
  else if (strcmp(argv[1], "source-file") == 0)
    {
      if (argc != 4)
        {
          fprintf(stderr, "If FILETYPE is \"source-file\" then FILENAME must be given\n");
          return 1;
        }
      rc = dbgserver_find_source((unsigned char *)argv[2], 0, argv[3], &cache_name);
    }
  else
    {
      fprintf(stderr, "Invalid filetype\n");
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
