/* Get Dwarf Frame state for target live PID process.
   Copyright (C) 2012 Red Hat, Inc.
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

#include "libdwflP.h"
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <dirent.h>

static bool
tid_is_attached (Dwfl *dwfl, pid_t tid)
{
  for (Dwfl_Frame_State_Process *process = dwfl->framestatelist; process;
       process = process->next)
    for (Dwfl_Frame_State_Thread *thread = process->thread; thread;
         thread = thread->next)
      if (thread->tid_attached && thread->tid == tid)
	return true;
  return false;
}

static bool
ptrace_attach (pid_t tid)
{
  if (ptrace (PTRACE_ATTACH, tid, NULL, NULL) != 0)
    return false;
  /* FIXME: Handle missing SIGSTOP on old Linux kernels.  */
  for (;;)
    {
      int status;
      if (waitpid (tid, &status, __WALL) != tid || !WIFSTOPPED (status))
	{
	  ptrace (PTRACE_DETACH, tid, NULL, NULL);
	  return false;
	}
      if (WSTOPSIG (status) == SIGSTOP)
	break;
      if (ptrace (PTRACE_CONT, tid, NULL,
		  (void *) (uintptr_t) WSTOPSIG (status)) != 0)
	{
	  ptrace (PTRACE_DETACH, tid, NULL, NULL);
	  return false;
	}
    }
  return true;
}

static bool
dwfl_frame_state_pid_memory_read (Dwarf_Addr addr, Dwarf_Addr *result,
				  void *user_data)
{
  Dwfl_Frame_State_Process *process = user_data;
  assert (process->core == NULL && process->thread->tid);
  if (process->ebl->class == ELFCLASS64)
    {
      errno = 0;
      *result = ptrace (PTRACE_PEEKDATA, process->thread->tid,
			(void *) (uintptr_t) addr, NULL);
      if (errno != 0)
	{
	  __libdwfl_seterrno (DWFL_E_PROCESS_MEMORY_READ);
	  return false;
	}
      return true;
    }
#if SIZEOF_LONG == 8
  /* We do not care about reads unaliged to 4 bytes boundary.
     But 0x...ffc read of 8 bytes could overrun a page.  */
  bool lowered = (addr & 4) != 0;
  if (lowered)
    addr -= 4;
#endif /* SIZEOF_LONG == 8 */
  errno = 0;
  *result = ptrace (PTRACE_PEEKDATA, process->thread->tid,
		    (void *) (uintptr_t) addr, NULL);
  if (errno != 0)
    {
      __libdwfl_seterrno (DWFL_E_PROCESS_MEMORY_READ);
      return false;
    }
#if SIZEOF_LONG == 8
# if BYTE_ORDER == BIG_ENDIAN
  if (! lowered)
    *result >>= 32;
# else
  if (lowered)
    *result >>= 32;
# endif
#endif /* SIZEOF_LONG == 8 */
  *result &= 0xffffffff;
  return true;
}

Dwfl_Frame_State *
dwfl_frame_state_pid (Dwfl *dwfl, pid_t pid)
{
  char dirname[64];
  int i = snprintf (dirname, sizeof (dirname), "/proc/%ld/task", (long) pid);
  assert (i > 0 && i < (ssize_t) sizeof (dirname) - 1);
  Dwfl_Frame_State_Process *process;
  process = __libdwfl_process_alloc (dwfl, dwfl_frame_state_pid_memory_read,
				     NULL);
  if (process == NULL)
    return NULL;
  process->memory_read_user_data = process;
  for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL; mod = mod->next)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	continue;
      process->ebl = mod->ebl;
    }
  if (process->ebl == NULL)
    {
      /* Not identified EBL from any of the modules.  */
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_PROCESS_NO_ARCH);
      return NULL;
    }
  DIR *dir = opendir (dirname);
  if (dir == NULL)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_PARSE_PROC);
      return NULL;
    }
  for (;;)
    {
      errno = 0;
      struct dirent *dirent = readdir (dir);
      if (dirent == NULL)
	{
	  if (errno == 0)
	    break;
	  __libdwfl_process_free (process);
	  __libdwfl_seterrno (DWFL_E_PARSE_PROC);
	  return NULL;
	}
      if (strcmp (dirent->d_name, ".") == 0
	  || strcmp (dirent->d_name, "..") == 0)
	continue;
      char *end;
      errno = 0;
      long tidl = strtol (dirent->d_name, &end, 10);
      if (errno != 0)
	{
	  __libdwfl_process_free (process);
	  __libdwfl_seterrno (DWFL_E_PARSE_PROC);
	  return NULL;
	}
      pid_t tid = tidl;
      if (tidl <= 0 || (end && *end) || tid != tidl)
	{
	  __libdwfl_process_free (process);
	  __libdwfl_seterrno (DWFL_E_PARSE_PROC);
	  return NULL;
	}
      Dwfl_Frame_State_Thread *thread = __libdwfl_thread_alloc (process, tid);
      if (thread == NULL)
	{
	  __libdwfl_process_free (process);
	  __libdwfl_seterrno (DWFL_E_NOMEM);
	  return NULL;
	}
      if (! tid_is_attached (dwfl, thread->tid))
	{
	  if (! ptrace_attach (thread->tid))
	    {
	      __libdwfl_thread_free (thread);
	      continue;
	    }
	  thread->tid_attached = true;
	}
      Dwfl_Frame_State *state = thread->unwound;
      if (! ebl_frame_state (state) || ! __libdwfl_state_fetch_pc (state))
	{
	  __libdwfl_thread_free (thread);
	  continue;
	}
    }
  if (closedir (dir) != 0)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_PARSE_PROC);
      return NULL;
    }
  if (process->thread == NULL)
    {
      /* No valid threads recognized.  */
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_NO_THREAD);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_pid)
