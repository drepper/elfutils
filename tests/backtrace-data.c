/* Test program for unwinding of frames.
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
#include <fcntl.h>
#include <string.h>
#include ELFUTILS_HEADER(dwfl)

#ifndef __x86_64__

int
main (void)
{
  return 77;
}

#else /* __x86_64__ */

static int
find_elf (Dwfl_Module *mod __attribute__ ((unused)),
	  void **userdata __attribute__ ((unused)),
	  const char *modname __attribute__ ((unused)),
	  Dwarf_Addr base __attribute__ ((unused)),
	  char **file_name __attribute__ ((unused)),
	  Elf **elfp __attribute__ ((unused)))
{
  /* Not used as modules are reported explicitly.  */
  assert (0);
}

static bool
memory_read (Dwarf_Addr addr, Dwarf_Addr *result, void *user_data)
{
  pid_t child = (uintptr_t) user_data;

  errno = 0;
  long l = ptrace (PTRACE_PEEKDATA, child, (void *) (uintptr_t) addr, NULL);
  assert_perror (errno);
  *result = l;

  /* We could also return false for failed ptrace.  */
  return true;
}

/* Return filename and VMA address *BASEP where its mapping starts which
   contains ADDR.  */

static char *
maps_lookup (pid_t pid, Dwarf_Addr addr, GElf_Addr *basep)
{
  char *fname;
  int i = asprintf (&fname, "/proc/%ld/maps", (long) pid);
  assert_perror (errno);
  assert (i > 0);
  FILE *f = fopen (fname, "r");
  assert_perror (errno);
  assert (f);
  free (fname);
  for (;;)
    {
      // 37e3c22000-37e3c23000 rw-p 00022000 00:11 49532 /lib64/ld-2.14.90.so */
      unsigned long start, end, offset;
      i = fscanf (f, "%lx-%lx %*s %lx %*x:%*x %*x", &start, &end, &offset);
      assert_perror (errno);
      assert (i == 3);
      char *filename = strdup ("");
      assert (filename);
      size_t filename_len = 0;
      for (;;)
	{
	  int c = fgetc (f);
	  assert (c != EOF);
	  if (c == '\n')
	    break;
	  if (c == ' ' && *filename == '\0')
	    continue;
	  filename = realloc (filename, filename_len + 2);
	  assert (filename);
	  filename[filename_len++] = c;
	  filename[filename_len] = '\0';
	}
      if (start <= addr && addr < end)
	{
	  i = fclose (f);
	  assert_perror (errno);
	  assert (i == 0);

	  *basep = start - offset;
	  return filename;
	}
      free (filename);
    }
}

/* Add module containing ADDR to the DWFL address space.  */

static void
report_module (Dwfl *dwfl, pid_t child, Dwarf_Addr addr)
{
  GElf_Addr base;
  char *long_name = maps_lookup (child, addr, &base);
  Dwfl_Module *mod = dwfl_report_elf_baseaddr (dwfl, long_name, long_name, -1,
					       base);
  assert (mod);
  free (long_name);
  assert (dwfl_addrmodule (dwfl, addr) == mod);
}

int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  pid_t child = fork ();
  switch (child)
  {
    case -1:
      assert_perror (errno);
      assert (0);
    case 0:;
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
      raise (SIGUSR1);
      assert (0);
    default:
      break;
  }

  int status;
  pid_t pid = waitpid (child, &status, 0);
  assert_perror (errno);
  assert (pid == child);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);

  struct user_regs_struct user_regs;
  long l = ptrace (PTRACE_GETREGS, child, NULL, &user_regs);
  assert_perror (errno);
  assert (l == 0);

  const unsigned nregs = 17;
  const uint64_t regs_set = (1U << nregs) - 1;
  Dwarf_Addr regs[17];
  regs[0] = user_regs.rax;
  regs[1] = user_regs.rdx;
  regs[2] = user_regs.rcx;
  regs[3] = user_regs.rbx;
  regs[4] = user_regs.rsi;
  regs[5] = user_regs.rdi;
  regs[6] = user_regs.rbp;
  regs[7] = user_regs.rsp;
  regs[8] = user_regs.r8;
  regs[9] = user_regs.r9;
  regs[10] = user_regs.r10;
  regs[11] = user_regs.r11;
  regs[12] = user_regs.r12;
  regs[13] = user_regs.r13;
  regs[14] = user_regs.r14;
  regs[15] = user_regs.r15;
  regs[16] = user_regs.rip;

  /* x86_64 has PC contained in its CFI subset of DWARF register set so
     elfutils will figure out the real PC value from REGS.  */
  const bool pc_set = false;
  Dwarf_Addr pc = 0;

  void *memory_read_user_data = (void *) (uintptr_t) child;

  static char *debuginfo_path;
  static const Dwfl_Callbacks offline_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,
      .section_address = dwfl_offline_section_address,
      .find_elf = find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&offline_callbacks);
  assert (dwfl);

  report_module (dwfl, child, user_regs.rip);

  Dwfl_Frame_State *state;
  state = dwfl_frame_state_data (dwfl, pc_set, pc, nregs, &regs_set, regs,
				 memory_read, memory_read_user_data);
  assert (state != NULL);

  /* Multiple threads are not handled here.  */
  do
    {
      bool minusone;
      if (! dwfl_frame_state_pc (state, &pc, &minusone))
	error (1, 0, "dwfl_frame_state_pc: %s", dwfl_errmsg (-1));
      Dwarf_Addr pc_adjusted = pc - (minusone ? 1 : 0);

      printf ("%#" PRIx64 "\n", (uint64_t) pc);

      Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
      if (mod == NULL)
	report_module (dwfl, child, pc_adjusted);

      if (! dwfl_frame_unwind (&state))
	error (1, 0, "dwfl_frame_unwind: %s", dwfl_errmsg (-1));
    }
  while (state);

  dwfl_end (dwfl);
  kill (child, SIGKILL);
  pid = waitpid (child, &status, 0);
  assert_perror (errno);
  assert (pid == child);
  assert (WIFSIGNALED (status));
  assert (WTERMSIG (status) == SIGKILL);

  return EXIT_SUCCESS;
}

#endif /* x86_64 */
