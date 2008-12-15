/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2005, 2006, 2007, 2008 Red Hat, Inc.
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

#include "libdwflP.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../libdw/libdwP.h"	/* DWARF_E_* values are here.  */

/* Find the main ELF file for this module and open libelf on it.
   When we return success, MOD->main is set up.  MOD->elferr is
   set up in any case. */
static Dwfl_Error
find_file (Dwfl_Module *mod)
{
  if (mod->main.cberr != DWFL_E_NOERROR)
    return mod->main.cberr;
  if (mod->main.shared != NULL)        /* Already done.  */
    return mod->main.shared->elferr;

  Elf *elf = NULL;
  mod->main.name = NULL;
  int fd = (*mod->dwfl->callbacks->find_elf) (MODCB_ARGS (mod),
					      &mod->main.name,
					      &elf);

  Dwfl_Error err = DWFL_E_NOERROR;

  /* If the callback didn't open the dwfl_file for us, but gave us at
     least indices, we will do it ourselves. */
  if (mod->main.shared == NULL)
    {
      if (unlikely (fd < 0 && mod->main.name == NULL && elf == NULL))
	/* The callback didn't give us anything... */
	return mod->main.cberr = DWFL_E_CB;

      err = __libdwfl_open_file (&mod->main, mod->main.name, fd, elf);
    }

  if (likely (err == DWFL_E_NOERROR))
    {
      if (unlikely (elf_kind (mod->main.shared->elf) != ELF_K_ELF))
	{
	  err = DWFL_E_BADELF;
	elf_error:
	  __libdwfl_close_file (&mod->main);
	  mod->main.name = NULL;
	  return err;
	}

      /* Clear any explicitly reported build ID, just in case it was
	 wrong.  We'll fetch it from the file when asked. */
      free (mod->build_id);
      mod->build_id = NULL;

      /* The following code duplicate to dwfl_report_elf.c. */
      mod->bias = ((mod->low_addr & -mod->main.shared->align)
		   - (mod->main.shared->start & -mod->main.shared->align));

      GElf_Ehdr ehdr_mem;
      GElf_Ehdr *ehdr = gelf_getehdr (mod->main.shared->elf, &ehdr_mem);
      if (ehdr == NULL)
	{
	  /* XXX This shouldn't happen, we already got ehdr in
	     __libdwfl_open_file.  Maybe put the the ehdr in shared
	     struct, since we extract it anyway?  */
	  err = DWFL_E (LIBELF, elf_errno ());
	  goto elf_error;
	}

      mod->e_type = ehdr->e_type;

      /* Relocatable Linux kernels are ET_EXEC but act like ET_DYN.  */
      if (mod->e_type == ET_EXEC
	  && mod->bias != 0)
	mod->e_type = ET_DYN;
    }

  return mod->main.cberr = err;
}

/* Search an ELF file for a ".gnu_debuglink" section.  */
static const char *
find_debuglink (Elf *elf, GElf_Word *crc)
{
  size_t shstrndx;
  if (elf_getshstrndx (elf, &shstrndx) < 0)
    return NULL;

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	return NULL;

      const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);
      if (name == NULL)
	return NULL;

      if (!strcmp (name, ".gnu_debuglink"))
	break;
    }

  if (scn == NULL)
    return NULL;

  /* Found the .gnu_debuglink section.  Extract its contents.  */
  Elf_Data *rawdata = elf_rawdata (scn, NULL);
  if (rawdata == NULL)
    return NULL;

  Elf_Data crcdata =
    {
      .d_type = ELF_T_WORD,
      .d_buf = crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };
  Elf_Data conv =
    {
      .d_type = ELF_T_WORD,
      .d_buf = rawdata->d_buf + rawdata->d_size - sizeof *crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return NULL;

  Elf_Data *d = gelf_xlatetom (elf, &crcdata, &conv, ehdr->e_ident[EI_DATA]);
  if (d == NULL)
    return NULL;
  assert (d == &crcdata);

  return rawdata->d_buf;
}


/* Find the separate debuginfo file for this module and open libelf on it.
   When we return success, MOD->debug is set up.  */
static Dwfl_Error
find_debuginfo (Dwfl_Module *mod)
{
  if (mod->debug.cberr != DWFL_E_NOERROR)
    return mod->debug.cberr;
  if (mod->debug.shared != NULL)        /* Already done.  */
    return mod->debug.shared->elferr;

  GElf_Word debuglink_crc = 0;
  const char *debuglink_file = find_debuglink (mod->main.shared->elf, &debuglink_crc);

  int fd = (*mod->dwfl->callbacks->find_debuginfo) (MODCB_ARGS (mod),
						    mod->main.name,
						    debuglink_file,
						    debuglink_crc,
						    &mod->debug.name);
  if (mod->debug.shared != NULL)
    return DWFL_E_NOERROR;

  if (fd < 0)
    return mod->debug.cberr = DWFL_E_CB;

  return mod->debug.cberr = __libdwfl_open_file (&mod->debug, mod->debug.name,
						 fd, NULL);
}


/* Try to open a libebl backend for MOD.  */
Dwfl_Error
internal_function
__libdwfl_module_getebl (Dwfl_Module *mod)
{
  if (mod->main.shared == NULL
      || mod->main.shared->ebl == NULL)
    {
      if (find_file (mod) != DWFL_E_NOERROR)
	return mod->main.cberr;

      mod->main.shared->ebl = ebl_openbackend (mod->main.shared->elf);
      if (mod->main.shared->ebl == NULL)
	return DWFL_E_LIBEBL;
    }
  return DWFL_E_NOERROR;
}

static Dwfl_Error
find_symfile (Dwfl_Module *mod)
{
  if (mod->symfile != NULL)
    return DWFL_E_NOERROR;

  Dwfl_Error error = find_file (mod);
  if (error != DWFL_E_NOERROR)
    return error;

  error = __libdwfl_find_symtab (mod->main.shared);
  if (error == DWFL_E_NO_SYMTAB
      || (error == DWFL_E_NOERROR && !mod->main.shared->is_symtab))
    {
      error = find_debuginfo (mod);
      if (error == DWFL_E_NOERROR)
	{
	  error = __libdwfl_find_symtab (mod->debug.shared);
	  if (error == DWFL_E_NOERROR)
	    {
	      /* .dynsym in debuginfo makes no sense, so if there is a
		 symtab, it's the proper one. */
	      mod->symfile = &mod->debug;
	      return error;
	    }
	}
      else if (mod->main.shared->symerr != DWFL_E_NOERROR)
	/* No .dynsym in main, so report failure to find debuginfo.  */
	return error;
    }
  if (error == DWFL_E_NOERROR)
    mod->symfile = &mod->main;
  return error;
}

/* Try to start up libdw on DEBUGFILE.  */
static Dwfl_Error
load_dw (Dwfl_Module *mod, struct dwfl_shared_file *debugfile)
{
  if (debugfile->dw != NULL			/* Already done. */
      || debugfile->dwerr != DWFL_E_NOERROR)	/* Cached previous failure. */
    return debugfile->dwerr;

  if (mod->e_type == ET_REL && !debugfile->relocated)
    {
      const Dwfl_Callbacks *const cb = mod->dwfl->callbacks;

      /* The debugging sections have to be relocated.  */
      if (cb->section_address == NULL)
	return DWFL_E_NOREL;

      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	return error;

      error = __libdwfl_relocate (mod, debugfile, true);
      if (error != DWFL_E_NOERROR)
	return error;
    }

  debugfile->dw = INTUSE(dwarf_begin_elf) (debugfile->elf, DWARF_C_READ, NULL);
  if (debugfile->dw == NULL)
    {
      int err = INTUSE(dwarf_errno) ();
      return err == DWARF_E_NO_DWARF ? DWFL_E_NO_DWARF : DWFL_E (LIBDW, err);
    }

  /* Until we have iterated through all CU's, we might do lazy lookups.  */
  mod->lazycu = 1;

  return DWFL_E_NOERROR;
}

/* Try to start up libdw on either the main file or the debuginfo file.  */
static Dwfl_Error
find_dw (Dwfl_Module *mod)
{
  if (mod->dwerr != DWFL_E_NOERROR) /* Cached failure to setup debug file. */
    return mod->dwerr;

  if (mod->debug.shared != NULL
      && (mod->debug.shared->dw != NULL /* Already done.  */
	  || mod->debug.shared->dwerr != DWFL_E_NOERROR)) /* Cached failure. */
    return mod->debug.shared->dwerr;

  mod->dwerr = find_file (mod);
  if (mod->dwerr != DWFL_E_NOERROR)
    return mod->dwerr;

  /* First see if the main ELF file has the debugging information.  */
  mod->main.shared->dwerr = load_dw (mod, mod->main.shared);
  switch (mod->main.shared->dwerr)
    {
    case DWFL_E_NOERROR:
      mod->debug.shared = mod->main.shared;
      return DWFL_E_NOERROR;

    case DWFL_E_NO_DWARF:
      break;

    default:
      return (mod->main.shared->dwerr
	      = __libdwfl_canon_error (mod->main.shared->dwerr));
    }

  /* Now we have to look for a separate debuginfo file.  */
  mod->dwerr = find_debuginfo (mod);
  switch (mod->dwerr)
    {
    case DWFL_E_NOERROR:
      return (mod->debug.shared->dwerr
	      = __libdwfl_canon_error (load_dw (mod, mod->debug.shared)));

    case DWFL_E_CB:		/* The find_debuginfo hook failed.  */
      return mod->dwerr = DWFL_E_NO_DWARF;

    default:
      return mod->dwerr = __libdwfl_canon_error (mod->dwerr);
    }
}


Elf *
dwfl_module_getelf (Dwfl_Module *mod, GElf_Addr *loadbase)
{
  if (mod == NULL)
    return NULL;

  if (find_file (mod) == DWFL_E_NOERROR)
    {
      if (mod->e_type == ET_REL && ! mod->main.shared->relocated)
	{
	  /* Before letting them get at the Elf handle,
	     apply all the relocations we know how to.  */

	  mod->main.shared->relocated = true;
	  if (likely (__libdwfl_module_getebl (mod) == DWFL_E_NOERROR))
	    {
	      (void) __libdwfl_relocate (mod, mod->main.shared, false);

	      if (mod->debug.shared == mod->main.shared)
		mod->debug.shared->relocated = true;
	      else if (mod->debug.shared != NULL && ! mod->debug.shared->relocated)
		{
		  mod->debug.shared->relocated = true;
		  (void) __libdwfl_relocate (mod, mod->debug.shared, false);
		}
	    }
	}

      *loadbase = mod->bias;
      return mod->main.shared->elf;
    }

  __libdwfl_seterrno (mod->main.cberr);
  return NULL;
}
INTDEF (dwfl_module_getelf)


Dwarf *
dwfl_module_getdwarf (Dwfl_Module *mod, Dwarf_Addr *bias)
{
  if (mod == NULL)
    return NULL;

  Dwfl_Error err = find_dw (mod);
  if (err == DWFL_E_NOERROR)
    {
      /* If dwfl_module_getelf was used previously, then partial apply
	 relocation to miscellaneous sections in the debug file too.  */
      if (mod->e_type == ET_REL
	  && mod->main.shared->relocated && ! mod->debug.shared->relocated)
	{
	  mod->debug.shared->relocated = true;
	  if (mod->debug.shared->elf != mod->main.shared->elf)
	    (void) __libdwfl_relocate (mod, mod->debug.shared, false);
	}

      *bias = DWBIAS (mod);
      return mod->debug.shared->dw;
    }

  __libdwfl_seterrno (err);
  return NULL;
}
INTDEF (dwfl_module_getdwarf)

int
dwfl_module_getsymtab (Dwfl_Module *mod)
{
  if (mod == NULL
      || mod->main.cberr != DWFL_E_NOERROR)
    /* Don't mind error in debug.cberr, symtab may be in main. */
    return -1;

  Dwfl_Error err = find_symfile(mod);
  if (mod->symfile == NULL)
    {
      __libdwfl_seterrno (err);
      return -1;
    }

  return mod->symfile->shared->syments;
}
INTDEF (dwfl_module_getsymtab)
