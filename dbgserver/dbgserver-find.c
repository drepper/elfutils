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

#include "config.h"
#include "dbgserver-client.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>


int
main(int argc, char** argv)
{
  if (argc < 3 || argc > 4)
    {
      fprintf(stderr, "%s (%s) %s\n", argv[0], PACKAGE_NAME, PACKAGE_VERSION);
      fprintf(stderr, "Usage: %s debuginfo BUILDID\n", argv[0]);
      fprintf(stderr, "       %s executable BUILDID\n", argv[0]);
      fprintf(stderr, "       %s source BUILDID /FILENAME\n", argv[0]);
      return 1;
    }

  int rc;
  char *cache_name;

  /* Check whether FILETYPE is valid and call the appropriate
     dbgserver_find_* function. If FILETYPE is "source"
     then ensure a FILENAME was also supplied as an argument.  */
  if (strcmp(argv[1], "debuginfo") == 0)
    rc = dbgserver_find_debuginfo((unsigned char *)argv[2], 0, &cache_name);
  else if (strcmp(argv[1], "executable") == 0)
    rc = dbgserver_find_executable((unsigned char *)argv[2], 0, &cache_name);
  else if (strcmp(argv[1], "source") == 0)
    {
      if (argc != 4 || argv[3][0] != '/')
        {
          fprintf(stderr, "If FILETYPE is \"source\" then absolute /FILENAME must be given\n");
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
