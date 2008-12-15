/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2005, 2006, 2007 Red Hat, Inc.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include <config.h>
#include "../libelf/libelfP.h"  /* For elf->map_address. */
#undef _

#include "libdwflP.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

struct cache_key
{
  dev_t dev;
  ino64_t ino;
  struct timespec ctim;
  size_t refcount;
};

struct cache_entry
{
  struct dwfl_shared_file shared;
  struct cache_key key;

  struct cache_entry *next;
};

static struct cache_entry *cache = NULL;

static struct cache_entry *
lookup_entry (struct stat64 *s)
{
  for (struct cache_entry *entry = cache; entry != NULL; entry = entry->next)
    if (entry->key.dev == s->st_dev
	&& entry->key.ino == s->st_ino
	&& entry->key.ctim.tv_nsec == s->st_ctim.tv_nsec
	&& entry->key.ctim.tv_sec == s->st_ctim.tv_sec)
      return entry;
  return NULL;
}

Dwfl_Error
internal_function
__libdwfl_open_file (struct dwfl_file *tgt,
		     const char *file_name,
		     int fd, Elf *elf)
{
  if (tgt->shared != NULL)
    return tgt->shared->elferr;

  Dwfl_Error error;
  if (fd < 0
      && elf == NULL
      && unlikely ((fd = open64 (file_name, O_RDONLY)) < 0))
    {
      tgt->shared = NULL;
      return DWFL_E (ERRNO, errno);
    }

  struct cache_entry *entry = NULL;
  bool synthetic = false;
  struct stat64 s;

  /* If we get there and fd < 0, it's ELF without file backing. */
  if (fd >= 0)
    {
      if (unlikely (fstat64 (fd, &s) < 0))
	{
	  tgt->shared = NULL;
	  return DWFL_E (ERRNO, errno);
	}

      /* Reuse cache entry if possible. */
      entry = lookup_entry (&s);
      if (entry != NULL)
	{
	  /* Consume elf. */
	  if (elf != NULL)
	    elf_end (elf);

	  /* Consume the fd.  We don't need it when reusing. */
	  if (fd >= 0 && unlikely (close (fd) < 0))
	    {
	      tgt->shared = NULL;
	      return DWFL_E (ERRNO, errno);
	    }

	  ++entry->key.refcount;
	  tgt->shared = &entry->shared;
	  return entry->shared.elferr;
	}
    }
  else
    synthetic = true;

  /* Failing that, enlist a new entry. */
  entry = calloc (1, sizeof *entry);
  if (unlikely (entry == NULL))
    {
      error = DWFL_E_NOMEM;
      goto fail;
    }

  entry->next = cache;
  cache = entry;

  /* For synthetic files, the key is left initialized to zero. */
  if (!synthetic)
    {
      entry->key.dev = s.st_dev;
      entry->key.ino = s.st_ino;
      entry->key.ctim = s.st_ctim;
    }
  entry->key.refcount = 1;

  tgt->shared = &entry->shared;
  tgt->name = file_name ? strdup (file_name) : NULL;

  if (elf == NULL)
    {
      elf = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
      if (unlikely (elf == NULL))
	{
	  error = DWFL_E_LIBELF;
	  goto fail;
	}
    }
  if (!synthetic
      && elf->map_address != NULL
      && elf_cntl (elf, ELF_C_FDREAD) == 0)
    {
      close (fd);
      fd = -1;
    }
  entry->shared.elf = elf;
  entry->shared.fd = fd;

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (entry->shared.elf, &ehdr_mem);
  if (unlikely (ehdr == NULL))
    {
      error = DWFL_E_LIBELF;
      goto fail;
    }

  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr ph_mem;
      GElf_Phdr *ph = gelf_getphdr (entry->shared.elf, i, &ph_mem);
      if (ph == NULL)
	{
	  error = DWFL_E_LIBELF;
	  goto fail;
	}
      if (ph->p_type == PT_LOAD)
	{
	  entry->shared.start = ph->p_vaddr;
	  entry->shared.align = ph->p_align;
	  break;
	}
    }

  return entry->shared.elferr = DWFL_E_NOERROR;


  /* Upon failure, keep the shared file open, but cache error. */
 fail:
  if (fd != -1)
    {
      /* Consume the FD, even if the caller opened it.  We don't check
	 for success, because if it failed, which error should we pass up? */
      close (fd);
      entry->shared.fd = -1;
    }

  if (elf != NULL)
    {
      elf_end (elf);
      entry->shared.elf = NULL;
    }

  error = __libdwfl_canon_error (error);
  if (entry != NULL)
    entry->shared.elferr = error;
  return error;
}

void
internal_function
__libdwfl_close_file (struct dwfl_file *tgt)
{
  if (likely (tgt->shared != NULL))
    {
      /* Look up the file in cache.  With singly linked list, we can't
	 simply cast file to entry, we need the prev pointer. */
      struct cache_entry *entry;
      struct cache_entry **prevp = &cache;
      for (entry = cache; entry != NULL;
	   entry = *(prevp = &entry->next))
	if (&entry->shared == tgt->shared)
	  break;
      assert (entry != NULL);

      if (--entry->key.refcount == 0)
	{
	  if (entry->shared.elf != NULL)
	    elf_end (entry->shared.elf);
	  if (entry->shared.fd != -1)
	    close (entry->shared.fd);

	  if (tgt->shared->dw != NULL)
	    INTUSE(dwarf_end) (tgt->shared->dw);

	  if (tgt->shared->ebl != NULL)
	    ebl_closebackend (tgt->shared->ebl);

	  *prevp = entry->next;
	  free (entry);

	  if (BUILD_ID_PTR (entry->shared.build_id))
	    free (entry->shared.build_id);
	}

      tgt->shared = NULL;
    }

  if (tgt->name != NULL)
    {
      free (tgt->name);
      tgt->name = NULL;
    }
}
