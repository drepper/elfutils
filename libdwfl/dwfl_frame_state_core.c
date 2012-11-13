/* Get Dwarf Frame state for target core file.
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
#include <fcntl.h>
#include "system.h"

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static bool
dwfl_frame_state_core_memory_read (Dwarf_Addr addr, Dwarf_Addr *result,
				   void *user_data)
{
  Dwfl_Frame_State_Process *process = user_data;
  Elf *core = process->core;
  assert (core != NULL);
  Dwfl *dwfl = process->dwfl;
  static size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return false;
    }
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_LOAD)
	continue;
      /* Bias is zero here, a core file itself has no bias.  */
      GElf_Addr start = __libdwfl_segment_start (dwfl, phdr->p_vaddr);
      GElf_Addr end = __libdwfl_segment_end (dwfl,
					     phdr->p_vaddr + phdr->p_memsz);
      unsigned bytes = process->ebl->class == ELFCLASS64 ? 8 : 4;
      if (addr < start || addr + bytes > end)
	continue;
      Elf_Data *data;
      data = elf_getdata_rawchunk (core, phdr->p_offset + addr - start,
				   bytes, ELF_T_ADDR);
      if (data == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return false;
	}
      assert (data->d_size == bytes);
      /* FIXME: Currently any arch supported for unwinding supports
	 unaligned access.  */
      if (bytes == 8)
	*result = *(const uint64_t *) data->d_buf;
      else
	*result = *(const uint32_t *) data->d_buf;
      return true;
    }
  __libdwfl_seterrno (DWFL_E_CORE_MISSING);
  return false;
}

Dwfl_Frame_State *
dwfl_frame_state_core (Dwfl *dwfl, const char *corefile)
{
  Dwfl_Frame_State_Process *process;
  process = __libdwfl_process_alloc (dwfl, dwfl_frame_state_core_memory_read,
				     NULL);
  if (process == NULL)
    return NULL;
  process->memory_read_user_data = process;
  int core_fd = open64 (corefile, O_RDONLY);
  if (core_fd < 0)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  process->core_fd = core_fd;
  Elf *core;
  Dwfl_Error err = __libdw_open_file (&core_fd, &core, true, false);
  if (err != DWFL_E_NOERROR)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (err);
      return NULL;
    }
  process->core = core;
  Ebl *ebl = ebl_openbackend (core);
  if (ebl == NULL)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  process->ebl = ebl;
  process->ebl_close = true;
  size_t nregs = ebl_frame_state_nregs (ebl);
  if (nregs == 0)
    {
      /* We do not support unwinding this CORE file EBL.  */
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return NULL;
    }
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (core, &ehdr_mem);
  if (ehdr == NULL)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  assert (ehdr->e_type == ET_CORE);
  size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    {
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }
  Dwfl_Frame_State_Thread *thread = NULL;
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset,
					     phdr->p_filesz, ELF_T_NHDR);
      if (data == NULL)
	{
	  __libdwfl_process_free (process);
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
	      /* This note may be just not recognized, skip it.  */
	      continue;
	    }
	  if (nhdr.n_type == NT_PRSTATUS)
	    {
	      const Ebl_Core_Item *item;
	      for (item = items; item < items + nitems; item++)
		if (strcmp (item->name, "pid") == 0)
		  break;
	      if (item == items + nitems)
		{
		  __libdwfl_process_free (process);
		  __libdwfl_seterrno (DWFL_E_BADELF);
		  return NULL;
		}
	      uint32_t val32 = *(const uint32_t *) (desc + item->offset);
	      val32 = (elf_getident (core, NULL)[EI_DATA] == ELFDATA2MSB
			? be32toh (val32) : le32toh (val32));
	      pid_t tid = (int32_t) val32;
	      eu_static_assert (sizeof val32 <= sizeof tid);
	      if (thread)
		{
		  /* Delay initialization of THREAD till all notes for it have
		     been read in.  */
		  Dwfl_Frame_State *state = thread->unwound;
		  if (! ebl_frame_state (state)
		      || ! __libdwfl_state_fetch_pc (state))
		    {
		      __libdwfl_thread_free (thread);
		      thread = NULL;
		      continue;
		    }
		}
	      thread = __libdwfl_thread_alloc (process, tid);
	      if (thread == NULL)
		{
		  __libdwfl_process_free (process);
		  __libdwfl_seterrno (DWFL_E_NOMEM);
		  return NULL;
		}
	    }
	  if (thread == NULL)
	    {
	      /* Ignore notes before first PR_NTSTATUS.  */
	      continue;
	    }
	  Dwfl_Frame_State *state = thread->unwound;
	  desc += regs_offset;
	  for (size_t regloci = 0; regloci < nregloc; regloci++)
	    {
	      const Ebl_Register_Location *regloc = reglocs + regloci;
	      if (regloc->regno >= nregs)
		continue;
	      assert (regloc->bits == 32 || regloc->bits == 64);
	      const char *reg_desc = desc + regloc->offset;
	      for (unsigned regno = regloc->regno;
		   regno < MIN (regloc->regno + (regloc->count ?: 1U), nregs);
		   regno++)
		{
		  /* PPC provides DWARF register 65 irrelevant for
		     CFI which clashes with register 108 (LR) we need.
		     LR (108) is provided earlier (in NT_PRSTATUS) than the # 65.
		     FIXME: It depends now on their order in core notes.  */
		  if (dwfl_frame_state_reg_get (state, regno, NULL))
		    continue;
		  Dwarf_Addr val;
		  switch (regloc->bits)
		  {
		    case 32:;
		      uint32_t val32 = *(const uint32_t *) reg_desc;
		      reg_desc += sizeof val32;
		      val32 = (elf_getident (core, NULL)[EI_DATA] == ELFDATA2MSB
			       ? be32toh (val32) : le32toh (val32));
		      /* Do a host width conversion.  */
		      val = val32;
		      break;
		    case 64:;
		      uint64_t val64 = *(const uint64_t *) reg_desc;
		      reg_desc += sizeof val64;
		      val64 = (elf_getident (core, NULL)[EI_DATA] == ELFDATA2MSB
			       ? be64toh (val64) : le64toh (val64));
		      assert (sizeof (*state->regs) == sizeof val64);
		      val = val64;
		      break;
		    default:
		      abort ();
		  }
		  /* Registers not valid for CFI are just ignored.  */
		  dwfl_frame_state_reg_set (state, regno, val);
		  reg_desc += regloc->pad;
		}
	    }
	}
    }
  if (thread)
    {
      /* Delay initialization of THREAD till all notes for it have been read
	 in.  */
      Dwfl_Frame_State *state = thread->unwound;
      if (! ebl_frame_state (state) || ! __libdwfl_state_fetch_pc (state))
	__libdwfl_thread_free (thread);
    }
  if (process->thread == NULL)
    {
      /* No valid threads recognized in this CORE.  */
      __libdwfl_process_free (process);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }
  return process->thread->unwound;
}
INTDEF (dwfl_frame_state_core)
