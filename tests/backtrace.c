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
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include "../libdwfl/libdwflP.h" /* for DWFL_E_RA_UNDEFINED ? */
#include ELFUTILS_HEADER(dwfl)

static Dwfl *
get_dwfl (pid_t pid)
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

  return dwfl;
}

static void
dump (pid_t pid)
{
  Dwfl *dwfl = get_dwfl (pid);
  Dwarf_Frame_State *state = dwfl_frame_state (dwfl);
  if (state == NULL)
    error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
  while (state)
    {
      Dwarf_Addr pc = dwarf_frame_state_pc (state);
      int dw_errno = dwarf_errno ();
      if (dw_errno != DWARF_E_NOERROR)
	error (2, 0, "dwarf_frame_state_pc: %s", dwarf_errmsg (dw_errno));
      printf ("%p\n", (void *) (intptr_t) pc);
      state = dwfl_frame_unwind (state);
      if (state == NULL)
	{
	  int dwf_errno = dwfl_errno ();
	  if (dwf_errno == DWFL_E_RA_UNDEFINED)
	    break;
	  error (2, 0, "dwfl_frame_unwind: %s", dwfl_errmsg (dwf_errno));
	}
    }

  dwfl_end (dwfl);
}

struct see_exec_module
{
  Dwfl_Module *mod;
  char selfpath[PATH_MAX + 1];
};

static int
see_exec_module (Dwfl_Module *mod, void **userdata __attribute__ ((unused)), const char *name __attribute__ ((unused)), Dwarf_Addr start __attribute__ ((unused)), void *arg)
{
  struct see_exec_module *data = arg;
  if (strcmp (name, data->selfpath) != 0)
    return DWARF_CB_OK;
  assert (data->mod == NULL);
  data->mod = mod;
  return DWARF_CB_OK;
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
	  errno = 0;
	  long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
	  assert_perror (errno);
	  assert (l == 0);
	  int i = raise (SIGUSR1);
	  /* Catch the .plt jump, it will come from __errno_location.  */
	  assert_perror (errno);
	  assert (i == 0);
	  abort ();
	default:
	  break;
      }
      Dwfl *dwfl = get_dwfl (pid);
      struct see_exec_module data;
      ssize_t ssize = readlink ("/proc/self/exe", data.selfpath, sizeof (data.selfpath));
      assert (ssize > 0 && ssize < sizeof (data.selfpath));
      data.selfpath[ssize] = '\0';
      data.mod = NULL;
      ptrdiff_t ptrdiff = dwfl_getmodules (dwfl, &see_exec_module, &data, 0);
      assert (ptrdiff == 0);
      assert (data.mod != NULL);
      GElf_Addr loadbase;
      Elf *elf = dwfl_module_getelf (data.mod, &loadbase);
      GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
      assert (ehdr != NULL);
      Elf_Scn *scn = NULL, *plt = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr scn_shdr_mem, *scn_shdr = gelf_getshdr (scn, &scn_shdr_mem);
	  assert (scn_shdr != NULL);
	  /* FIXME: sh_type */
	  if (strcmp (elf_strptr (elf, ehdr->e_shstrndx, scn_shdr->sh_name), ".plt") != 0)
	    continue;
	  assert (plt == NULL);
	  plt = scn;
	}
      assert (plt != NULL);
      GElf_Shdr scn_shdr_mem, *scn_shdr = gelf_getshdr (plt, &scn_shdr_mem);
      assert (scn_shdr != NULL);
      Dwarf_Addr plt_start = scn_shdr->sh_addr + loadbase;
      Dwarf_Addr plt_end = plt_start + scn_shdr->sh_size;
      dwfl_end (dwfl);
      errno = 0;
      int status;
      pid_t got = waitpid (pid, &status, 0);
      assert_perror (errno);
      assert (got == pid);
      assert (WIFSTOPPED (status));
      assert (WSTOPSIG (status) == SIGUSR1);
      for (;;)
	{
	  long l;
	  errno = 0;
#if defined __x86_64__
	  l = ptrace (PTRACE_PEEKUSER, pid, (void *) (intptr_t) offsetof (struct user_regs_struct, rip), NULL);
#elif defined __i386__
	  l = ptrace (PTRACE_PEEKUSER, pid, (void *) (intptr_t) offsetof (struct user_regs_struct, eip), NULL);
#else
	  l = 0;
#endif
	  assert_perror (errno);
	  if ((unsigned long) l >= plt_start && (unsigned long) l < plt_end)
	    break;
	  l = ptrace (PTRACE_SINGLESTEP, pid, NULL, NULL);
	  assert_perror (errno);
	  assert (l == 0);
	  got = waitpid (pid, &status, 0);
	  assert_perror (errno);
	  assert (got == pid);
	  assert (WIFSTOPPED (status));
	  assert (WSTOPSIG (status) == SIGTRAP);
	}
      long l = ptrace (PTRACE_DETACH, pid, NULL, (void *) (intptr_t) SIGSTOP);
      assert_perror (errno);
      assert (l == 0);
      got = waitpid (pid, &status, WSTOPPED);
      assert_perror (errno);
      assert (got == pid);
      assert (WIFSTOPPED (status));
      assert (WSTOPSIG (status) == SIGSTOP);
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
