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

#include <libelf.h>
#include "libelfP.h"
#include "common.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Chdr) *
elfw2(LIBELFBITS,getchdr) (Elf_Scn *scn, int *type)
{
  /* Do we have the header already?  */
  ElfW2(LIBELFBITS,Chdr) *chdr = scn->chdr.ELFW(e,LIBELFBITS);
  if (chdr != NULL)
    {
      *type = scn->chdr_type;
      return chdr != (void *) -1 ? chdr : NULL;
    }

  ElfW2(LIBELFBITS,Shdr) *shdr;
  shdr = elfw2(LIBELFBITS,getshdr) (scn);
  if (shdr == NULL)
    {
    error:
      *type = -1;
      return NULL;
    }

  /* Allocated or no bits sections can never be compressed.  */
  if ((shdr->sh_flags & SHF_ALLOC) != 0
      || shdr->sh_type == SHT_NULL || shdr->sh_type == SHT_NOBITS)
    goto not_compressed;

  Elf_Data *d;
  d = elf_getdata (scn, NULL);
  if (d == NULL)
    goto error;

  /* Deal with either a real Chdr or the old GNU zlib format.  */
  if (d->d_size >= sizeof (ElfW2(LIBELFBITS,Chdr))
      && (shdr->sh_flags & SHF_COMPRESSED) != 0)
    {
      Elf *elf = scn->elf;
      ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

      if (ehdr->e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || ((uintptr_t) scn->rawdata_base
		  & (__alignof__ (ElfW2(LIBELFBITS,Chdr)) - 1)) == 0))
	{
	  chdr = (ElfW2(LIBELFBITS,Chdr) *)scn->rawdata_base;
	  scn->chdr.ELFW(e,LIBELFBITS) = chdr;
	}
      else
	{
	  scn->chdr.ELFW(e,LIBELFBITS)
	    = malloc (sizeof (ElfW2(LIBELFBITS,Chdr)));
	  chdr = scn->chdr.ELFW(e,LIBELFBITS);
	  if (chdr == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      goto error;
	    }
	  if (ehdr->e_ident[EI_DATA] == MY_ELFDATA)
	    memcpy (chdr, d->d_buf, sizeof (ElfW2(LIBELFBITS,Chdr)));
	  else
	    {
	      ElfW2(LIBELFBITS,Chdr) *bchdr = d->d_buf;
	      CONVERT_TO (chdr->ch_type, bchdr->ch_type);
#if LIBELFBITS == 64
	      CONVERT_TO (chdr->ch_reserved, bchdr->ch_reserved);
#endif
	      CONVERT_TO (chdr->ch_size, bchdr->ch_size);
	      CONVERT_TO (chdr->ch_type, bchdr->ch_type);
	    }
	}
      *type = scn->chdr_type = ELF_ZSCN_T_ELF;
      return chdr;
    }
  else if (d->d_size >= 4 + 8
	   && memcmp (d->d_buf, "ZLIB", 4) == 0)
    {
      /* There is a 12-byte header of "ZLIB" followed by
	 an 8-byte big-endian size.  There is only one type and
	 Alignment isn't preserved separately.  */
      uint64_t size;
      memcpy (&size, d->d_buf + 4, sizeof size);
      size = be64toh (size);

      /* One more sanity check, size should be bigger than original
	 data size plus some overhead (4 chars ZLIB + 8 bytes size + 6
	 bytes zlib stream overhead + 5 bytes overhead max for one 16K
	 block) and should fit into a size_t.  */
      if (size + 4 + 8 + 6 + 5 < d->d_size || size > SIZE_MAX)
	goto not_compressed;

      scn->chdr.ELFW(e,LIBELFBITS) = malloc (sizeof (ElfW2(LIBELFBITS,Chdr)));
      chdr = scn->chdr.ELFW(e,LIBELFBITS);
      if (chdr == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto error;
	}

      *type = scn->chdr_type = ELF_ZSCN_T_GNU;
      chdr->ch_type = ELFCOMPRESS_ZLIB;
      chdr->ch_size = size;
      chdr->ch_addralign = shdr->sh_addralign;
      return chdr;
    }

not_compressed:
  scn->chdr.ELFW(e,LIBELFBITS) = (void *) -1;
  *type = scn->chdr_type = ELF_ZSCN_T_NONE;
  __libelf_seterrno (ELF_E_NOT_COMPRESSED);
  return NULL;
}
