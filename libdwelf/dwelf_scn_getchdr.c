/* Return section compression header.
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

#include "libdwelfP.h"
#include "libelfP.h"
#include <gelf.h>
#include "common.h"

DWElf_Chdr *
dwelf_scn_getchdr (Elf_Scn *scn, DWElf_Chdr *dest)
{
  if (scn == NULL || dest == NULL)
    return NULL;

  GElf_Ehdr ehdr;
  if (gelf_getehdr (scn->elf, &ehdr) == NULL)
    return NULL;

  GElf_Shdr shdr;
  if (gelf_getshdr (scn, &shdr) == NULL)
    return NULL;

  /* Allocated or no bits sections can never be compressed.  */
  if ((shdr.sh_flags & SHF_ALLOC) != 0
      || shdr.sh_type == SHT_NULL
      || shdr.sh_type == SHT_NOBITS
      || (shdr.sh_flags & SHF_COMPRESSED) == 0)
    return NULL;

  Elf_Data *data = elf_rawdata (scn, NULL);
  if (data == NULL)
    return NULL;

  if (scn->elf->class == ELFCLASS32)
    {
      if (data->d_size < sizeof (Elf32_Chdr))
	return NULL;

      Elf32_Chdr chdr;
      if (ehdr.e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) data->d_buf & (__alignof__ (Elf32_Chdr) - 1))
		  == 0)))
	chdr = *(Elf32_Chdr *) data->d_buf;
      else
	memcpy (&chdr, data->d_buf, sizeof (Elf32_Chdr));

      if (ehdr.e_ident[EI_DATA] == MY_ELFDATA)
	{
	  dest->ch_type = chdr.ch_type;
	  dest->ch_reserved = 0;
	  dest->ch_size = chdr.ch_size;
	  dest->ch_addralign = chdr.ch_addralign;
	}
      else
	{
	  CONVERT_TO (dest->ch_type, chdr.ch_type);
	  dest->ch_reserved = 0;
	  CONVERT_TO (dest->ch_size, chdr.ch_size);
	  CONVERT_TO (dest->ch_addralign, chdr.ch_addralign);
	}
    }
  else
    {
      if (data->d_size < sizeof (Elf64_Chdr))
	return NULL;

      if (ehdr.e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) data->d_buf & (__alignof__ (Elf64_Chdr) - 1))
		  == 0)))
	*dest = *(Elf64_Chdr *) data->d_buf;
      else
	{
	  memcpy (dest, data->d_buf, sizeof (Elf64_Chdr));
	  if (ehdr.e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (dest->ch_type);
	      CONVERT (dest->ch_reserved);
	      CONVERT (dest->ch_size);
	      CONVERT (dest->ch_addralign);
	    }
	}
    }

  return dest;
}
