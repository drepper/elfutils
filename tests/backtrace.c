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
#include <argp.h>
#include <fcntl.h>
#include <string.h>
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
dump (Dwfl *dwfl, pid_t pid, const char *corefile, void (*callback) (unsigned frameno, Dwarf_Addr pc, const char *symname, Dwfl *dwfl))
{
  if (pid)
    report_pid (dwfl, pid);
  Dwarf_Frame_State *state;
  if (pid)
    state = dwfl_frame_state_pid (dwfl, pid);
  else if (corefile)
    state = dwfl_frame_state_core (dwfl, corefile);
  else
    abort ();
  if (state == NULL)
    error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
  for (unsigned frameno = 0; state; frameno++)
    {
      Dwarf_Addr pc;
      bool minusone;
      if (! dwfl_frame_state_pc (state, &pc, &minusone))
	error (2, 0, "dwfl_frame_state_pc: %s", dwfl_errmsg (-1));
      Dwarf_Addr pc_adjusted = pc - (minusone ? 1 : 0);

      /* Get PC->SYMNAME.  */
      Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
      const char *symname = NULL;
      if (mod)
	symname = dwfl_module_addrname (mod, pc_adjusted);

      printf ("#%2u %#" PRIx64 "%4s\t%s\n", frameno, (uint64_t) pc, minusone ? "- 1" : "", symname);
      if (callback)
	callback (frameno, pc, symname, dwfl);
      if (! dwfl_frame_unwind (&state))
	error (2, 0, "dwfl_frame_unwind: %s", dwfl_errmsg (-1));
      if (state == NULL)
	break;
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
    case ARGP_KEY_SUCCESS:;
      Dwfl *dwfl = state->hook;
      if (dwfl)
	break;
      static const Dwfl_Callbacks callbacks =
	{
	  .find_elf = dwfl_linux_proc_find_elf,
	  .find_debuginfo = dwfl_standard_find_debuginfo,
	};
      dwfl = dwfl_begin (&callbacks);
      state->hook = dwfl;
      break;
    }
  return parse_opt_orig (key, arg, state);
}

/* Execution will arrive here from jmp by an artificial ptrace-spawn signal.  */

static void
sigusr2 (int signo)
{
  assert (signo == SIGUSR2);
  raise (SIGUSR1);

  /* Catch the .plt jump, it will come from this abort call.  */
  abort ();
}

static __attribute__ ((noinline, noclone)) void
dummy1 (void)
{
  asm volatile ("");
}

#if !defined __x86_64__ && !defined __i386__
static
#endif
__attribute__ ((noinline, noclone)) void
jmp (void)
{
  /* Not reached, signal will get ptrace-spawn to jump into sigusr2.  */
  abort ();
}

static __attribute__ ((noinline, noclone)) void
dummy2 (void)
{
  asm volatile ("");
}

static __attribute__ ((noinline, noclone, noreturn)) void
stdarg (int f, ...)
{
  sighandler_t sigusr2_orig = signal (SIGUSR2, sigusr2);
  assert (sigusr2_orig == SIG_DFL);
  errno = 0;
  long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
  assert_perror (errno);
  assert (l == 0);
#if defined __x86_64__ || defined __i386__
  raise (SIGUSR1);
  /* Execution will get PC patched into function jmp.  */
#else
  sigusr2 (SIGUSR2);
#endif
  abort ();
}

static __attribute__ ((noinline, noclone)) void
dummy3 (void)
{
  asm volatile ("");
}

static __attribute__ ((noinline, noclone)) void
backtracegen (void)
{
  stdarg (1);
  /* Here should be no instruction after the stdarg call as it is noreturn
     function.  It must be stdarg so that it is a call and not jump (jump as
     a tail-call).  */
}

static __attribute__ ((noinline, noclone)) void
dummy4 (void)
{
  asm volatile ("");
}

static void
selfdump_callback (unsigned frameno, Dwarf_Addr pc, const char *symname, Dwfl *dwfl)
{
#if defined __x86_64__ || defined __i386__
  Dwfl_Module *mod;
  const char *symname2 = NULL;
  switch (frameno)
  {
    case 0:
      /* .plt has no symbols.  */
      assert (symname == NULL);
      break;
    case 1:
      assert (symname != NULL && strcmp (symname, "sigusr2") == 0);
      break;
    case 2:
      /* __restore_rt - glibc maybe does not have to have this symbol.  */
      break;
    case 3:
      /* Verify we trapped on the very first instruction of jmp.  */
      assert (symname != NULL && strcmp (symname, "jmp") == 0);
      mod = dwfl_addrmodule (dwfl, pc - 1);
      if (mod)
	symname2 = dwfl_module_addrname (mod, pc - 1);
      assert (symname2 == NULL || strcmp (symname2, "jmp") != 0);
      break;
    case 4:
      assert (symname != NULL && strcmp (symname, "stdarg") == 0);
      break;
    case 5:
      /* Verify we trapped on the very last instruction of child.  */
      assert (symname != NULL && strcmp (symname, "backtracegen") == 0);
      mod = dwfl_addrmodule (dwfl, pc);
      if (mod)
	symname2 = dwfl_module_addrname (mod, pc);
      assert (symname2 == NULL || strcmp (symname2, "backtracegen") != 0);
      break;
  }
#endif /* __x86_64__ || __i386__ */
}

static void *
start (void *arg)
{
  backtracegen ();
  abort ();
}

static void
child (void)
{
  pthread_t thread;

  errno = 0;
  i = pthread_create (&thread, NULL, start, NULL);
  assert_perror (errno);
  assert (i == 0);
  long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
  assert_perror (errno);
  assert (l == 0);
  raise (SIGUSR2);
  abort ();
}

static void
prepare_thread (pid_t pid2)
{
  long l;
#if defined __x86_64__ || defined __i386__
  errno = 0;
#if defined __x86_64__
  l = ptrace (PTRACE_POKEUSER, pid2, (void *) (intptr_t) offsetof (struct user_regs_struct, rip), (void *) jmp);
#elif defined __i386__
  l = ptrace (PTRACE_POKEUSER, pid2, (void *) (intptr_t) offsetof (struct user_regs_struct, eip), (void *) jmp);
#else
# error
#endif
  assert_perror (errno);
  assert (l == 0);
  l = ptrace (PTRACE_CONT, pid2, NULL, (void *) (intptr_t) SIGUSR2);
  int status;
  pid_t got = waitpid (pid2, &status, 0);
  assert_perror (errno);
  assert (got == pid2);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);
  for (;;)
    {
      errno = 0;
#if defined __x86_64__
      l = ptrace (PTRACE_PEEKUSER, pid2, (void *) (intptr_t) offsetof (struct user_regs_struct, rip), NULL);
#elif defined __i386__
      l = ptrace (PTRACE_PEEKUSER, pid2, (void *) (intptr_t) offsetof (struct user_regs_struct, eip), NULL);
#else
# error
#endif
      assert_perror (errno);
      if ((unsigned long) l >= plt_start && (unsigned long) l < plt_end)
	break;
      l = ptrace (PTRACE_SINGLESTEP, pid2, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
      got = waitpid (pid2, &status, 0);
      assert_perror (errno);
      assert (got == pid2);
      assert (WIFSTOPPED (status));
      assert (WSTOPSIG (status) == SIGTRAP);
    }
#endif /* __x86_64__ || __i386__ */
}

static void
ptrace_detach_stopped (pid_t pid)
{
  errno = 0;
  /* This kill is needed for kernel-2.6.18-308.el5.ppc64.  */
  long l = kill (pid, SIGSTOP);
  assert_perror (errno);
  assert (l == 0);
  l = ptrace (PTRACE_DETACH, pid, NULL, (void *) (intptr_t) SIGSTOP);
  assert_perror (errno);
  assert (l == 0);
  siginfo_t siginfo;
  /* With kernel-2.6.18-308.el5.ppc64 we would get hanging waitpid after later PTRACE_ATTACH.  */
  l = waitid (P_PID, pid, &siginfo, WSTOPPED | WNOWAIT);
  assert_perror (errno);
  assert (l == 0);
  assert (siginfo.si_pid == pid);
  assert (siginfo.si_signo == SIGCHLD);
  assert (siginfo.si_code == CLD_STOPPED);
  /* kernel-2.6.18-308.el5.ppc64 has there WIFSTOPPED + WSTOPSIG,
     kernel-3.4.11-1.fc16.x86_64 has there the plain signal value.  */
  assert ((WIFSTOPPED (siginfo.si_status) && WSTOPSIG (siginfo.si_status) == SIGSTOP)
	  || siginfo.si_status == SIGSTOP);
}

static void
selfdump (Dwfl *dwfl)
{
  dummy1 ();
  dummy2 ();
  dummy3 ();
  dummy4 ();
  pid_t pid = fork ();
  switch (pid)
  {
    case -1:
      abort ();
    case 0:
      child ();
      abort ();
    default:
      break;
  }
  report_pid (dwfl, pid);
  struct see_exec_module data;
  ssize_t ssize = readlink ("/proc/self/exe", data.selfpath, sizeof (data.selfpath));
  assert (ssize > 0 && ssize < sizeof (data.selfpath));
  data.selfpath[ssize] = '\0';
  data.mod = NULL;
  ptrdiff_t ptrdiff = dwfl_getmodules (dwfl, see_exec_module, &data, 0);
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

  /* Catch the main thread.  */
  errno = 0;
  int status;
  pid_t got = waitpid (pid, &status, 0);
  assert_perror (errno);
  assert (got == pid);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);

  /* Catch the spawned thread.  */
  pid_t pid2 = waitpid (-1, &status, 0);
  assert_perror (errno);
  assert (pid2 > 0);
  assert (pid2 != pid);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR2);

  prepare_thread (pid2);
  /* T (Stopped) is per-PID (not per-TID) so maybe this is excessive.  */
  ptrace_detach_stopped (pid);
  ptrace_detach_stopped (pid2);

  dump (dwfl, pid, NULL, selfdump_callback);
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

  if (!pid && !corefile)
    selfdump (dwfl);
  else if (pid && !corefile)
    dump (dwfl, pid, NULL, NULL);
  else if (corefile && !pid)
    dump (dwfl, 0, corefile, NULL);
  else
    abort ();

  return 0;
}
