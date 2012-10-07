/* Test child for parent backtrace test.
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
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <string.h>
#include <pthread.h>

static int ptraceme, gencore;

/* Execution will arrive here from jmp by an artificial ptrace-spawn signal.  */

static void
sigusr2 (int signo)
{
  assert (signo == SIGUSR2);
  if (! gencore)
    raise (SIGUSR1);

  /* Catch the .plt jump, it will come from this abort call.  */
  abort ();
}

static __attribute__ ((noinline, noclone)) void
dummy1 (void)
{
  asm volatile ("");
}

#ifdef __x86_64__
static __attribute__ ((noinline, noclone)) void
jmp (void)
{
  /* Not reached, signal will get ptrace-spawn to jump into sigusr2.  */
  abort ();
}
#endif

static __attribute__ ((noinline, noclone)) void
dummy2 (void)
{
  asm volatile ("");
}

static __attribute__ ((noinline, noclone, noreturn)) void
stdarg (int f __attribute__ ((unused)), ...)
{
  sighandler_t sigusr2_orig = signal (SIGUSR2, sigusr2);
  assert (sigusr2_orig == SIG_DFL);
  errno = 0;
  if (ptraceme)
    {
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
    }
#ifdef __x86_64__
  if (! gencore)
    {
      /* Execution will get PC patched into function jmp.  */
      raise (SIGUSR1);
    }
#endif
  sigusr2 (SIGUSR2);
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

static void *
start (void *arg __attribute__ ((unused)))
{
  backtracegen ();
  abort ();
}

int
main (int argc __attribute__ ((unused)), char **argv)
{
  assert (*argv++);
  ptraceme = (*argv && strcmp (*argv, "--ptraceme") == 0);
  argv += ptraceme;
  gencore = (*argv && strcmp (*argv, "--gencore") == 0);
  argv += gencore;
  assert (*argv && strcmp (*argv, "--run") == 0);
  dummy1 ();
  dummy2 ();
  dummy3 ();
  dummy4 ();
  errno = 0;
  pthread_t thread;
  int i = pthread_create (&thread, NULL, start, NULL);
  assert_perror (errno);
  assert (i == 0);
  if (ptraceme)
    {
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
    }
  if (gencore)
    pthread_join (thread, NULL);
  else
    raise (SIGUSR2);
  abort ();
}
