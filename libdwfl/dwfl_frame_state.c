/* Get Dwarf Frame state for target PID or core file.
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
#include "../libdw/cfi.h"

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Exact copy from src/readelf.c.  */

static const void *
convert (Elf *core, Elf_Type type, uint_fast16_t count,
	 void *value, const void *data, size_t size)
{
  Elf_Data valuedata =
    {
      .d_type = type,
      .d_buf = value,
      .d_size = size ?: gelf_fsize (core, type, count, EV_CURRENT),
      .d_version = EV_CURRENT,
    };
  Elf_Data indata =
    {
      .d_type = type,
      .d_buf = (void *) data,
      .d_size = valuedata.d_size,
      .d_version = EV_CURRENT,
    };

  Elf_Data *d = (gelf_getclass (core) == ELFCLASS32
		 ? elf32_xlatetom : elf64_xlatetom)
    (&valuedata, &indata, elf_getident (core, NULL)[EI_DATA]);
  if (d == NULL)
    return NULL;

  return data + indata.d_size;
}

static bool
pid_is_attached (Dwfl *dwfl, pid_t pid)
{
  for (Dwarf_Frame_State_Base *base = dwfl->statebaselist; base; base = base->next)
    if (base->pid_attached && base->pid == pid)
      return true;
  return false;
}

Dwarf_Frame_State *
dwfl_frame_state (Dwfl *dwfl)
{
  if (dwfl == NULL)
    return NULL;

  /* Allocate STATE with proper backend-dependent size.  Possibly also fetch
     inferior registers if DWFL->PID is not zero.  */
  Dwarf_Frame_State *state = NULL;
  {
    Dwfl_Module *mod;
    __libdwfl_seterrno (DWFL_E_BADELF);
    for (mod = dwfl->modulelist; mod != NULL; mod = mod->next)
      {
	Dwfl_Error error = __libdwfl_module_getebl (mod);
	if (error != DWFL_E_NOERROR)
	  {
	    __libdwfl_seterrno (error);
	    continue;
	  }
	state = ebl_frame_state (mod->ebl, dwfl->pid, ! pid_is_attached (dwfl, dwfl->pid));
	if (state)
	  {
	    __libdwfl_seterrno (DWFL_E_NOERROR);
	    Dwarf_Frame_State_Base *base = state->base;
	    base->dwfl = dwfl;
	    base->next = dwfl->statebaselist;
	    dwfl->statebaselist = base;
	    break;
	  }
      }
  }
  if (state == NULL || dwfl->pid)
    return state;

  /* Fetch inferior registers from a core file.  */
  Dwfl_Module *coremod = NULL;
  {
    Dwfl_Module *mod;
    for (mod = dwfl->modulelist; mod != NULL; mod = mod->next)
      {
	GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (mod->main.elf, &ehdr_mem);
	if (ehdr == NULL)
	  {
	    __libdwfl_seterrno (DWFL_E_LIBELF);
	    return NULL;
	  }
	if (ehdr->e_type != ET_CORE)
	  continue;
	if (coremod != NULL)
	  {
	    /* More than one core files found.  */
	    __libdwfl_seterrno (DWFL_E_BADELF);
	    return NULL;
	  }
	coremod = mod;
      }
  }
  if (coremod == NULL)
    {
      /* No core file found.  */
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  Elf *core = coremod->main.elf;
  Dwarf_Frame_State_Base *base = state->base;
  base->core = core;
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (core, &ehdr_mem);
  assert (ehdr);
  assert (ehdr->e_type == ET_CORE);
  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
    {
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  static size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
      if (data == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return NULL;
	}
      size_t offset = 0;
      GElf_Nhdr nhdr;
      size_t name_offset;
      size_t desc_offset;
      while (offset < data->d_size
	     && (offset = gelf_getnote (data, offset,
					&nhdr, &name_offset, &desc_offset)) > 0)
	{
	  /* Do not check NAME for now, help broken Linux kernels.  */
	  const char *name = data->d_buf + name_offset;
	  const char *desc = data->d_buf + desc_offset;
	  GElf_Word regs_offset;
	  size_t nregloc;
	  const Ebl_Register_Location *reglocs;
	  size_t nitems;
	  const Ebl_Core_Item *items;
	  if (! ebl_core_note (ebl, &nhdr, name,
			       &regs_offset, &nregloc, &reglocs, &nitems, &items))
	    {
	      __libdwfl_seterrno (DWFL_E_LIBEBL);
	      return NULL;
	    }
	  desc += regs_offset;
	  for (size_t regloci = 0; regloci < nregloc; regloci++)
	    {
	      const Ebl_Register_Location *regloc = reglocs + regloci;
	      if (regloc->regno >= base->nregs)
		continue;
	      assert (regloc->bits == 32 || regloc->bits == 64);
	      const char *reg_desc = desc + regloc->offset;
	      for (unsigned regno = regloc->regno; regno < MIN (regloc->regno + (regloc->count ?: 1U), base->nregs); regno++)
		{
		  switch (regloc->bits)
		  {
		    case 32:
		      {
			uint32_t value;
			reg_desc = convert (core, ELF_T_WORD, 1, &value, reg_desc, 0);
			/* NULL REG_DESC is caught below.  */
			/* Do a possible host width conversion.  */
			state->regs[regno] = value;
		      }
		      break;
		    case 64:
		      {
			uint64_t value;
			reg_desc = convert (core, ELF_T_XWORD, 1, &value, reg_desc, 0);
			/* NULL REG_DESC is caught below.  */
			assert (sizeof (*state->regs) == sizeof (value));
			state->regs[regno] = value;
		      }
		      break;
		    default:
		      abort ();
		  }
		  if (reg_desc == NULL)
		    {
		      __libdwfl_seterrno (DWFL_E_BADELF);
		      return NULL;
		    }
		  state->regs_set |= 1U << regno;
		  reg_desc += regloc->pad;
		}
	    }
	}
    }
  return state;
}
INTDEF (dwfl_frame_state)
