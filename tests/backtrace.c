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

static Dwfl *
dwfl_get_proc (void)
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
  return dwfl;
}

static Dwfl *
dwfl_get_offline (void)
{
  static char *debuginfo_path;
  static const Dwfl_Callbacks offline_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,

      .section_address = dwfl_offline_section_address,

      /* We use this table for core files too.  */
      .find_elf = dwfl_build_id_find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&offline_callbacks);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));
  return dwfl;
}

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
report_core (Dwfl *dwfl, const char *corefile)
{
#if 0
  /* FIXME: Here COREFILE gets errorneously reported into VMA of DWFL.  */
  Dwfl_Module *mod = dwfl_report_elf (dwfl, "core", corefile, -1, 0 /* base */);
  assert (mod != NULL);
  GElf_Addr loadbase;
  Elf *core = dwfl_module_getelf (mod, &loadbase);
  assert (core != NULL);
  int result = dwfl_core_file_report (dwfl, core);
  assert (result >= 0);
#endif
#if 0
  /* FIXME: Here it does not build as __libdw_open_file is private.  */
  int core_fd = open64 (corefile, O_RDONLY);
  assert (core_fd >= 0);
  Elf *core;
  Dwfl_Error err = __libdw_open_file (&core_fd, &core, true, false);
  assert (err == DWFL_E_NOERROR);
  int result = dwfl_core_file_report (dwfl, core);
  assert (result >= 0);
  result = elf_end (core);
  assert (result == 0);
  result = close (core_fd);
  assert (result == 0);
#endif
#if 1
  /* FIXME: Here we use different DWFL, reported data would be used after free
     if we called dwfl_end (core_dwfl).  Therefore we leak CORE_DWFL.  */
  Dwfl *core_dwfl = dwfl_get_offline ();
  Dwfl_Module *mod = dwfl_report_elf (core_dwfl, "core", corefile, -1,
				      0 /* base */);
  assert (mod != NULL);
  GElf_Addr loadbase;
  Elf *core = dwfl_module_getelf (mod, &loadbase);
  assert (core != NULL);
  int result = dwfl_core_file_report (dwfl, core);
  assert (result >= 0);
  // dwfl_end (core_dwfl);
#endif

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));
}

static int
dump_modules (Dwfl_Module *mod, void **userdata __attribute__ ((unused)),
	      const char *name, Dwarf_Addr start,
	      void *arg __attribute__ ((unused)))
{
  Dwarf_Addr end;
  dwfl_module_info (mod, NULL, NULL, &end, NULL, NULL, NULL, NULL);
  printf ("%#" PRIx64 "\t%#" PRIx64 "\t%s\n", (uint64_t) start, (uint64_t) end,
	  name);
  return DWARF_CB_OK;
}

static void
dump (Dwfl *dwfl, pid_t pid, const char *corefile,
      void (*callback) (pid_t tid, unsigned frameno, Dwarf_Addr pc,
			const char *symname, Dwfl *dwfl, void *data),
      void *data)
{
  Dwarf_Frame_State *state;
  if (pid && !corefile)
    state = dwfl_frame_state_pid (dwfl, pid);
  else if (corefile && !pid)
    state = dwfl_frame_state_core (dwfl, corefile);
  else
    abort ();
  if (state == NULL)
    error (2, 0, "dwfl_frame_state: %s", dwfl_errmsg (-1));
  ptrdiff_t ptrdiff = dwfl_getmodules (dwfl, dump_modules, NULL, 0);
  assert (ptrdiff == 0);
  int err = 0;
  do
    {
      Dwarf_Frame_State *thread = state;
      pid_t tid = dwfl_frame_tid_get (thread);
      printf ("TID %ld:\n", (long) tid);
      unsigned frameno;
      for (frameno = 0; state; frameno++)
	{
	  Dwarf_Addr pc;
	  bool minusone;
	  if (! dwfl_frame_state_pc (state, &pc, &minusone))
	    {
	      fprintf (stderr, "%s\n", dwfl_errmsg (-1));
	      err = 1;
	      break;
	    }
	  Dwarf_Addr pc_adjusted = pc - (minusone ? 1 : 0);

	  /* Get PC->SYMNAME.  */
	  Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc_adjusted);
	  const char *symname = NULL;
	  if (mod)
	    symname = dwfl_module_addrname (mod, pc_adjusted);

	  printf ("#%2u %#" PRIx64 "%4s\t%s\n", frameno, (uint64_t) pc,
		  minusone ? "- 1" : "", symname);
	  if (callback)
	    callback (tid, frameno, pc, symname, dwfl, data);
	  if (! dwfl_frame_unwind (&state))
	    {
	      fprintf (stderr, "%s\n", dwfl_errmsg (-1));
	      err = 1;
	      break;
	    }
	}
      state = dwfl_frame_thread_next (thread);
    }
  while (state);
  if (callback)
    callback (0, 0, 0, NULL, dwfl, data);
  dwfl_end (dwfl);
  if (err)
    exit (EXIT_FAILURE);
}

struct see_exec_module
{
  Dwfl_Module *mod;
  char selfpath[PATH_MAX + 1];
};

static int
see_exec_module (Dwfl_Module *mod, void **userdata __attribute__ ((unused)),
		 const char *name __attribute__ ((unused)),
		 Dwarf_Addr start __attribute__ ((unused)), void *arg)
{
  struct see_exec_module *data = arg;
  if (strcmp (name, data->selfpath) != 0)
    return DWARF_CB_OK;
  assert (data->mod == NULL);
  data->mod = mod;
  return DWARF_CB_OK;
}

static void
selfdump_callback (pid_t tid, unsigned frameno, Dwarf_Addr pc,
		   const char *symname, Dwfl *dwfl, void *data)
{
  pid_t check_tid = (intptr_t) data;
  bool disable = check_tid < 0;
  if (disable)
    check_tid = -check_tid;
  static bool seen_main = false;
  if (symname && strcmp (symname, "main") == 0)
    seen_main = true;
  if (pc == 0)
    {
      assert (seen_main);
      return;
    }
  if (disable || tid != check_tid)
    return;
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
}

#ifdef __x86_64__
static void
prepare_thread (pid_t pid2, Dwarf_Addr plt_start, Dwarf_Addr plt_end,
		void (*jmp) (void))
{
  long l;
  errno = 0;
  l = ptrace (PTRACE_POKEUSER, pid2,
	      (void *) (intptr_t) offsetof (struct user_regs_struct, rip), jmp);
  assert_perror (errno);
  assert (l == 0);
  l = ptrace (PTRACE_CONT, pid2, NULL, (void *) (intptr_t) SIGUSR2);
  int status;
  pid_t got = waitpid (pid2, &status, __WALL);
  assert_perror (errno);
  assert (got == pid2);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);
  for (;;)
    {
      errno = 0;
      l = ptrace (PTRACE_PEEKUSER, pid2,
		  (void *) (intptr_t) offsetof (struct user_regs_struct, rip),
		  NULL);
      assert_perror (errno);
      if ((unsigned long) l >= plt_start && (unsigned long) l < plt_end)
	break;
      l = ptrace (PTRACE_SINGLESTEP, pid2, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
      got = waitpid (pid2, &status, __WALL);
      assert_perror (errno);
      assert (got == pid2);
      assert (WIFSTOPPED (status));
      assert (WSTOPSIG (status) == SIGTRAP);
    }
}
#endif /* __x86_64__ */

#include <asm/unistd.h>
#include <unistd.h>
#define tgkill(pid, tid, sig) syscall (__NR_tgkill, (pid), (tid), (sig))

static void
ptrace_detach_stopped (pid_t pid, pid_t group_pid)
{
  errno = 0;
  /* This kill is needed for kernel-2.6.18-308.el5.ppc64.  */
  long l = tgkill (group_pid, pid, SIGSTOP);
  assert_perror (errno);
  assert (l == 0);
  l = ptrace (PTRACE_DETACH, pid, NULL, (void *) (intptr_t) SIGSTOP);
  assert_perror (errno);
  assert (l == 0);
  /* Currently broken on kernel-3.5.4-2.fc17.x86_64.  */
#if 0
  siginfo_t siginfo;
  /* With kernel-2.6.18-308.el5.ppc64 we would get hanging waitpid after later
     PTRACE_ATTACH.  */
  l = waitid (P_PID, pid, &siginfo, WSTOPPED | WNOWAIT | WNOHANG);
  assert_perror (errno);
  assert (l == 0);
  assert (siginfo.si_pid == pid);
  assert (siginfo.si_signo == SIGCHLD);
  assert (siginfo.si_code == CLD_STOPPED);
  /* kernel-2.6.18-308.el5.ppc64 has there WIFSTOPPED + WSTOPSIG,
     kernel-3.4.11-1.fc16.x86_64 has there the plain signal value.  */
  assert ((WIFSTOPPED (siginfo.si_status)
	   && WSTOPSIG (siginfo.si_status) == SIGSTOP)
	  || siginfo.si_status == SIGSTOP);
#endif
}

static void
selfdump (Dwfl *dwfl, const char *exec)
{
  pid_t pid = fork ();
  switch (pid)
  {
    case -1:
      abort ();
    case 0:
      execl (exec, exec, "--ptraceme", "--run", NULL);
      abort ();
    default:
      break;
  }

  /* Catch the main thread.  Catch it first otherwise the /proc evaluation of
     PID may have caught still ourselves before executing execl above.  */
  errno = 0;
  int status;
  pid_t got = waitpid (pid, &status, 0);
  assert_perror (errno);
  assert (got == pid);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR2);

  /* Catch the spawned thread.  Do not use __WCLONE as we could get racy
     __WCLONE, probably despite pthread_create already had to be called the new
     task is not yet alive enough for waitpid.  */
  pid_t pid2 = waitpid (-1, &status, __WALL);
  assert_perror (errno);
  assert (pid2 > 0);
  assert (pid2 != pid);
  assert (WIFSTOPPED (status));
  assert (WSTOPSIG (status) == SIGUSR1);

  report_pid (dwfl, pid);
  char *selfpathname;
  int i = asprintf (&selfpathname, "/proc/%ld/exe", (long) pid);
  assert (i > 0);
  struct see_exec_module data;
  ssize_t ssize = readlink (selfpathname, data.selfpath,
			    sizeof (data.selfpath));
  free (selfpathname);
  assert (ssize > 0 && ssize < (ssize_t) sizeof (data.selfpath));
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
      if (strcmp (elf_strptr (elf, ehdr->e_shstrndx, scn_shdr->sh_name),
		  ".plt") != 0)
	continue;
      assert (plt == NULL);
      plt = scn;
    }
  assert (plt != NULL);
  GElf_Shdr scn_shdr_mem, *scn_shdr = gelf_getshdr (plt, &scn_shdr_mem);
  assert (scn_shdr != NULL);
  /* Make it true on x86_64 with i386 inferior.  */
  int disable = ehdr->e_ident[EI_CLASS] == ELFCLASS32;
#ifdef __x86_64__
  Dwarf_Addr plt_start = scn_shdr->sh_addr + loadbase;
  Dwarf_Addr plt_end = plt_start + scn_shdr->sh_size;
  void (*jmp) (void);
  if (! disable)
    {
      int nsym = dwfl_module_getsymtab (data.mod);
      int symi;
      for (symi = 1; symi < nsym; ++symi)
	{
	  GElf_Sym symbol;
	  const char *symbol_name = dwfl_module_getsym (data.mod, symi, &symbol, NULL);
	  if (symbol_name == NULL)
	    continue;
	  switch (GELF_ST_TYPE (symbol.st_info))
	    {
	    case STT_SECTION:
	    case STT_FILE:
	    case STT_TLS:
	      continue;
	    default:
	      if (strcmp (symbol_name, "jmp") != 0)
		continue;
	      break;
	    }
	  /* LOADBASE is already applied here.  */
	  jmp = (void (*) (void)) (uintptr_t) symbol.st_value;
	  break;
	}
      assert (symi < nsym);
    prepare_thread (pid2, plt_start, plt_end, jmp);
    }
#endif
  ptrace_detach_stopped (pid, pid);
  ptrace_detach_stopped (pid2, pid);
  report_pid (dwfl, pid);
  dump (dwfl, pid, NULL, selfdump_callback,
	(void *) (intptr_t) (disable ? -pid2 : pid2));
}

static bool
is_core (const char *corefile)
{
  Dwfl *dwfl = dwfl_get_offline ();
  Dwfl_Module *mod = dwfl_report_elf (dwfl, "core", corefile, -1, 0 /* base */);
  assert (mod != NULL);
  GElf_Addr loadbase;
  Elf *core = dwfl_module_getelf (mod, &loadbase);
  assert (core != NULL);
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (core, &ehdr_mem);
  assert (ehdr != NULL);
  assert (ehdr->e_type == ET_CORE || ehdr->e_type == ET_EXEC
	  || ehdr->e_type == ET_DYN);
  bool retval = ehdr->e_type == ET_CORE;
  dwfl_end (dwfl);
  return retval;
}

int
main (int argc __attribute__ ((unused)), char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  if (argc == 1)
    {
      selfdump (dwfl_get_proc (), "./backtrace-child");
      return 0;
    }
  argv++;
  if (argc == 2)
    {
      if (strcmp (*argv, "--help") == 0)
	error (2, 0, "backtrace {{no args for ./backtrace-child}|<pid>|<core>|"
		     "<executable>|<executable core>}");
      char *end;
      long l = strtol (*argv, &end, 10);
      if (**argv && !*end)
	{
	  Dwfl *dwfl = dwfl_get_proc ();
	  report_pid (dwfl, l);
	  dump (dwfl, l, NULL, NULL, NULL);
	}
      else if (is_core (*argv))
	{
	  Dwfl *dwfl = dwfl_get_offline ();
	  report_core (dwfl, *argv);
	  dump (dwfl, 0, *argv, NULL, NULL);
	}
      else
	selfdump (dwfl_get_proc (), *argv);
      return 0;
    }
  if (argc == 3)
    {
      assert (! is_core (argv[0]));
      assert (is_core (argv[1]));
      Dwfl *dwfl = dwfl_get_offline ();
      /* We must report core before the executable otherwise dwfl_addrmodule
	 will find the one from a core file which dwfl_module_addrname will not
	 work for.  */
      report_core (dwfl, argv[1]);
      Dwfl_Module *mod;
      mod = dwfl_report_offline (dwfl, "<executable>", argv[0], -1);
      assert (mod);
      dump (dwfl, 0, argv[1], NULL, NULL);
      return 0;
    }
  assert (0);

  return 0;
}
