/* Compress or decompress a section.
   Copyright (C) 2015 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libelfP.h"
#include <gelf.h>
#include <stddef.h>


GElf_Shdr *
gelf_zscn_compress (Elf_Scn *scn, GElf_Shdr *dest, int type, int ch_type)
{
  if (scn == NULL)
    return NULL;

  if (dest == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  if (scn->elf->class == ELFCLASS32)
    {
      Elf32_Shdr *shdr = elf32_zscn_compress (scn, type, ch_type);
      if (shdr == NULL)
	return NULL;

      dest->sh_name = shdr->sh_name;
      dest->sh_type = shdr->sh_type;
      dest->sh_flags = shdr->sh_flags;
      dest->sh_addr = shdr->sh_addr;
      dest->sh_offset = shdr->sh_offset;
      dest->sh_size = shdr->sh_size;
      dest->sh_link = shdr->sh_link;
      dest->sh_info = shdr->sh_info;
      dest->sh_addralign = shdr->sh_addralign;
      dest->sh_entsize = shdr->sh_entsize;
    }
  else
    {
      Elf64_Shdr *shdr = elf64_zscn_compress (scn, type, ch_type);
      if (shdr == NULL)
	return NULL;

      *dest = *shdr;
    }

  return dest;
}
INTDEF(gelf_zscn_compress)
