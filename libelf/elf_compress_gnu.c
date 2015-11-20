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

#include <libelf.h>
#include "libelfP.h"
#include "common.h"

int
elf_compress_gnu (Elf_Scn *scn, int inflate)
{
  if (scn == NULL)
    return -1;

  Elf *elf = scn->elf;
  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    return -1;

  int elfdata = ehdr.e_ident[EI_DATA];

  GElf_Shdr shdr;
  if (gelf_getshdr (scn, &shdr) == NULL)
    return -1;

  /* For GNU compression we cannot really know whether the section is
     already compressed or not.  Just try and see what happens...  */
  // int compressed = (shdr.sh_flags & SHF_COMPRESSED);
  if (inflate == 1)
    {
      size_t hsize = 4 + 8; /* GNU "ZLIB" + 8 byte size.  */
      size_t orig_size, new_size, orig_addralign;
      void *out_buf = __libelf_compress (scn, hsize, elfdata,
					 &orig_size, &orig_addralign,
					 &new_size);
      if (out_buf == NULL)
	return -1;

      uint64_t be64_size = htobe64 (orig_size);
      memmove (out_buf, "ZLIB", 4);
      memmove (out_buf + 4, &be64_size, sizeof (be64_size));

      /* We don't know anything about sh_entsize, sh_addralign and
	 sh_flags won't have a SHF_COMPRESSED hint in the GNU format.
	 Just adjust the sh_size.  */
      shdr.sh_size = new_size;

      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, out_buf, new_size, 1, ELF_T_BYTE);

      /* The section is now compressed, we could keep the uncompressed
	 data around, but since that might have been multiple Elf_Data
	 buffers let the user uncompress it explicitly again if they
	 want it to simplify bookkeeping.  */
      scn->zdata_base = NULL;

      return 0;
    }
  else if (inflate == 0)
    {
      /* In theory the user could have constucted a compressed section
	 by hand.  But we always just take the rawdata directly and
	 decompress that.  */
      Elf_Data *data = elf_rawdata (scn, NULL);
      if (data == NULL)
	return -1;

      size_t hsize = 4 + 8; /* GNU "ZLIB" + 8 byte size.  */
      if (data->d_size < hsize || memcmp (data->d_buf, "ZLIB", 4) != 0)
	{
          __libelf_seterrno (ELF_E_NOT_COMPRESSED);
	  return -1;
	}

      /* There is a 12-byte header of "ZLIB" followed by
	 an 8-byte big-endian size.  There is only one type and
	 Alignment isn't preserved separately.  */
      uint64_t gsize;
      memcpy (&gsize, data->d_buf + 4, sizeof gsize);
      gsize = be64toh (gsize);

      /* One more sanity check, size should be bigger than original
	 data size plus some overhead (4 chars ZLIB + 8 bytes size + 6
	 bytes zlib stream overhead + 5 bytes overhead max for one 16K
	 block) and should fit into a size_t.  */
      if (gsize + 4 + 8 + 6 + 5 < data->d_size || gsize > SIZE_MAX)
	{
	  __libelf_seterrno (ELF_E_NOT_COMPRESSED);
	  return -1;
	}

      size_t size = gsize;
      size_t size_in = data->d_size - hsize;
      void *buf_in = data->d_buf + hsize;
      void *buf_out = __libelf_decompress (buf_in, size_in, size);
      if (buf_out == NULL)
	return -1;

      /* We don't know anything about sh_entsize, sh_addralign and
	 sh_flags won't have a SHF_COMPRESSED hint in the GNU format.
	 Just adjust the sh_size.  */
      shdr.sh_size = size;

      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, buf_out, size, shdr.sh_addralign,
			      __libelf_data_type (elf, shdr.sh_type));

      scn->zdata_base = buf_out;

      return 0;
    }
  else
    {
      __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
      return -1;
    }

}
