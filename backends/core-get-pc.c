/* Common core note PC address extraction.
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

#include <stdlib.h>

static bool
core_get_pc (Elf *core, Dwarf_Addr *core_pc, unsigned pc_offset)
{
  size_t phnum;
  if (elf_getphdrnum (core, &phnum) < 0)
    return NULL;
  unsigned bits = gelf_getclass (core) == ELFCLASS32 ? 32 : 64;
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
      if (phdr == NULL || phdr->p_type != PT_NOTE)
	continue;
      Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset,
					     phdr->p_filesz, ELF_T_NHDR);
      if (data == NULL)
	return NULL;
      size_t offset = 0;
      GElf_Nhdr nhdr;
      size_t name_offset;
      size_t desc_offset;
      while (offset < data->d_size
	     && (offset = gelf_getnote (data, offset,
					&nhdr, &name_offset, &desc_offset)) > 0)
	{
	  if (nhdr.n_type != NT_PRSTATUS)
	    continue;
	  const char *reg_desc = data->d_buf + desc_offset + pc_offset;
	  if (reg_desc + bits / 8 > (const char *) data->d_buf + nhdr.n_descsz)
	    continue;
	  Dwarf_Addr val;
	  switch (bits)
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
	      val = val64;
	      break;
	    default:
	      abort ();
	  }
	  *core_pc = val;
	  return true;
	}
    }
  return false;
}
