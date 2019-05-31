/* Test program for fetching debuginfo with debuginfo-server.
   Copyright (C) 2019 Red Hat, Inc.
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
#include <stdio.h>
#include ELFUTILS_HEADER(dwfl)
#include <elf.h>
#include <dwarf.h>
#include "../libdwfl/debuginfoserver-client.c"
#include <argp.h>
#include <assert.h>

#define MAX_BUILD_ID_BYTES 64

static int debuginfo_fd;

static int
get_build_id_local (Dwfl_Module *mod,
                    void **user __attribute__ ((unused)), 
                    const char *mod_name __attribute__ ((unused)),
                    Dwarf_Addr low_addr __attribute__ ((unused)),
                    void *arg __attribute__ ((unused)))
{
  size_t len;
  const unsigned char *bits;
  GElf_Addr vaddr; 

  len = dwfl_module_build_id(mod, &bits, &vaddr);
  assert(len > 0);

  debuginfo_fd = dbgserver_find_debuginfo(bits, len);

  return DWARF_CB_OK;
}

int
main (int argc, char *argv[])
{
  int remaining;
  Dwfl *dwfl;
  error_t res;

  assert(dbgserver_enabled());

  res = argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining, &dwfl);
  assert (res == 0 && dwfl != NULL);

  ptrdiff_t off = 0;
  do
    off = dwfl_getmodules (dwfl, get_build_id_local, NULL, off); 
  while (off > 0);

  /* TODO: ensure build-ids match.  */
  if (debuginfo_fd < 0)
    return 1;

  return 0;
}
