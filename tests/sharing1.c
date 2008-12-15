/* Copyright (C) 2007 Red Hat, Inc.
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

#include ELFUTILS_HEADER(dwflP)
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int bogus_find_elf (Dwfl_Module *, void **,
			   const char *, Dwarf_Addr,
			   char **, Elf **);

static char *debuginfo_path;
static const Dwfl_Callbacks proc_callbacks =
{
  .find_debuginfo = dwfl_standard_find_debuginfo,
  .debuginfo_path = &debuginfo_path,

  .find_elf = dwfl_linux_proc_find_elf,
};

static const Dwfl_Callbacks bogus_callbacks =
{
  .find_debuginfo = dwfl_standard_find_debuginfo,
  .debuginfo_path = &debuginfo_path,

  .find_elf = bogus_find_elf,
};

struct testdata
{
  Dwfl_Module *module;
  char *name;
  Elf *elf;
};


static int
bogus_find_elf (Dwfl_Module *mod __attribute__ ((unused)),
		void **userdata __attribute__ ((unused)),
		const char *modname __attribute__ ((unused)),
		Dwarf_Addr base __attribute__ ((unused)),
		char ** file_name __attribute__ ((unused)),
		Elf ** elf __attribute__ ((unused)))
{
  char * fn = strdup (".");
  *file_name = fn;
  int fd = open(fn, O_RDONLY);
  return fd;
}

int
each_module (Dwfl_Module *module,
	     void **userdata __attribute__ ((unused)),
	     const char *name,
	     Dwarf_Addr base __attribute__ ((unused)),
	     void *arg)
{
  struct testdata *t = (struct testdata *)arg;

  GElf_Addr bias;
  Elf *elf = dwfl_module_getelf (module, &bias);

  if (elf == NULL
      || dwfl_module_getsymtab (module) <= 0)
    return DWARF_CB_OK; /* Continue iteration until first ordinary
			   mapping with symbol table is found. */
  else
    {
      t->name = strdup (name);
      t->elf = elf;
      t->module = module;
      return DWARF_CB_ABORT;
    }
}

Dwfl *
initdwfl (pid_t pid, struct testdata *t, Dwfl_Callbacks const *cb, bool check)
{
  Dwfl *dwfl = dwfl_begin (cb);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));

  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));

  t->module = NULL;
  t->elf = NULL;
  t->name = NULL;
  dwfl_getmodules (dwfl, each_module, (void*)t, 0);
  if (check && (t->elf == NULL || t->name == NULL || t->module == NULL))
    error (2, 0, "couldn't init testdata.");

  return dwfl;
}

int
proc_test (void)
{
  struct testdata ta;
  Dwfl *a = initdwfl (getpid(), &ta, &proc_callbacks, true);

  struct testdata tb;
  Dwfl *b = initdwfl (getpid(), &tb, &proc_callbacks, true);

  if (ta.module == tb.module)
    error (2, 0, "Strange.  Both dwfls contain the same module?");

  if (strcmp (ta.name, tb.name))
    error (2, 0, "ELF files from different files.");

  if (ta.elf != tb.elf)
    error (2, 0, "ELF files not shared.");

  if (ta.module->symfile->shared->symdata != tb.module->symfile->shared->symdata)
    error (2, 0, "Symdata not shared.");

  dwfl_end (a);
  dwfl_end (b);
  return 0;
}

int
bogus_test (void)
{
  struct testdata ta;
  Dwfl *a = initdwfl (getpid(), &ta, &bogus_callbacks, false);
  dwfl_end (a);
  return 0;
}

int
main (void)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  return proc_test ()
    | bogus_test ();
}
