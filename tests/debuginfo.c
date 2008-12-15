/* Copyright (C) 2008 Red Hat, Inc.
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

#include ELFUTILS_HEADER(dwfl)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static char *filename = NULL;
static char *dfilename = NULL;
bool was_there = false;

static char *debuginfo_path = NULL;

static void
die (const char *message)
{
  fprintf (stderr, "%s\n", message);
  exit (1);
}

int
my_find_debuginfo (Dwfl_Module *mod,
		   void **userdata,
		   const char *modname,
		   GElf_Addr base,
		   const char *file_name,
		   const char *debuglink_file,
		   GElf_Word debuglink_crc,
		   char **debuginfo_file_name)
{
  int ret = dwfl_standard_find_debuginfo (mod, userdata, modname, base,
					  file_name, debuglink_file,
					  debuglink_crc, debuginfo_file_name);

  if (*debuginfo_file_name == NULL
      || strcmp (*debuginfo_file_name, dfilename))
    die ("Unexpected debuginfo found.");

  was_there = true;
  return ret;
}

static const Dwfl_Callbacks my_callbacks =
{
  .find_debuginfo = my_find_debuginfo,
  .debuginfo_path = &debuginfo_path,
};

int
main(int argc, char ** argv)
{
  if (argc != 3)
    die ("Usage: debuginfo <binary> <matching debuginfo>\n");
  filename = argv[1];
  dfilename = argv[2];

  Dwfl *dwfl = dwfl_begin (&my_callbacks);
  if (dwfl == NULL)
    die ("Couldn't create dwfl.");
  dwfl_report_begin (dwfl);
  Dwfl_Module *mod1 = dwfl_report_elf (dwfl, "mod1", filename, -1, 0);
  if (mod1 == NULL)
    die ("Couldn't create a module.");
  dwfl_report_end (dwfl, NULL, NULL);

  const unsigned char *bits;
  GElf_Addr vaddr;
  GElf_Addr bias;
  dwfl_module_getelf (mod1, &bias);
  int bytes = dwfl_module_build_id (mod1, &bits, &vaddr);
  if (bytes != 20)
    die ("Expected 20 bytes of debuginfo.");

  Dwarf_Die *d = dwfl_module_nextcu (mod1, NULL, &bias);
  if (!was_there)
    die ("Suspicious: find_debuginfo hook not called.");
  if (d == NULL)
    die ("No dwarf die found in debuginfo.");

  dwfl_end (dwfl);

  return 0;
}
