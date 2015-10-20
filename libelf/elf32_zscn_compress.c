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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Shdr) *
elfw2(LIBELFBITS,zscn_compress) (Elf_Scn *scn, int type, int ch_type)
{
  if (scn == NULL)
    return NULL;

  ElfW2(LIBELFBITS,Shdr) *shdr;
  shdr = elfw2(LIBELFBITS,getshdr) (scn);
  if (shdr == NULL)
    return NULL;

  int compressed;
  ElfW2(LIBELFBITS,Chdr) *chdr;
  chdr = elfw2(LIBELFBITS,getchdr) (scn, &compressed);
  if (chdr == NULL && compressed == -1)
    return NULL;

  if (compressed == ELF_ZSCN_T_NONE)
    {
      if (type != ELF_ZSCN_T_GNU && type != ELF_ZSCN_T_ELF)
	{
	  __libelf_seterrno (ELF_E_NOT_COMPRESSED);
	  return NULL;
	}

      if (ch_type != ELFCOMPRESS_ZLIB)
	{
	  __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
	  return NULL;
	}

      /* The compressed data is the raw/unconverted data.  We simplify
	 the implementation a bit by asking for the (converted) data
	 and then convert back to raw if needed.  Should be made a bit
	 more clever to directly use raw if that is all that is
	 available.  */
      Elf_Data *data = elf_getdata (scn, NULL);
      if (data == NULL)
	return NULL;

      /* Guess an output size. 1/8th of the original sh_size at least 8K.  */
      size_t block = shdr->sh_size / 8;
      if (block < 8 * 1024)
	block = 8 * 1024;
      size_t out_size = block;
      void *out_buf = malloc (out_size);
      if (out_buf == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  return NULL;
	}
      /* Reserve space for the GNU/ELF Chdr header.  */
      size_t hsize = (type == ELF_ZSCN_T_GNU
		      ? 4 + 8 /* GNU "ZLIB" + 8 byte size.  */
		      : sizeof (ElfW2(LIBELFBITS,Chdr)));
      size_t used = hsize;

      z_stream z;
      z.zalloc = Z_NULL;
      z.zfree = Z_NULL;
      z.opaque = Z_NULL;
      int zrc = deflateInit (&z, Z_BEST_COMPRESSION);
      if (zrc != Z_OK)
	{
	  __libelf_seterrno (ELF_E_COMPRESS_ERROR);
	  return NULL;
	}

      /* Cleanup and NULL return on error.  Don't leak memory.  */
      void *deflate_error (int err)
      {
	__libelf_seterrno (err);
	deflateEnd (&z);
	free (out_buf);
	return NULL;
      }

      /* Loop over data buffers.  */
      int flush;
      size_t orig_size = 0;
      do
	{
	  /* XXX convert to raw if different endianess.  */
	  orig_size += data->d_size;
	  z.avail_in = data->d_size;
	  z.next_in = data->d_buf;
	  data = elf_getdata (scn, data);
	  flush = data == NULL ? Z_FINISH : Z_NO_FLUSH;
	  /* Flush one data buffer.  */
	  do
	    {
	      z.avail_out = out_size - used;
	      z.next_out = out_buf + used;
	      zrc = deflate (&z, flush);
	      if (zrc == Z_STREAM_ERROR)
		return deflate_error (ELF_E_COMPRESS_ERROR);
	      used += (out_size - used) - z.avail_out;
	      if (z.avail_out == 0)
		{
		  void *bigger = realloc (out_buf, out_size + block);
		  if (bigger == NULL)
		    return deflate_error (ELF_E_NOMEM);
		  out_buf = bigger;
		  out_size += block;
		}
	    }
	  while (z.avail_out == 0); /* Need more output buffer.  */
	}
      while (flush != Z_FINISH); /* More data blocks.  */

      zrc = deflateEnd (&z);
      if (zrc != Z_OK)
	return deflate_error (ELF_E_COMPRESS_ERROR);

      if (type == ELF_ZSCN_T_GNU)
	{
	  uint64_t be64_size = htobe64 (orig_size);
	  memmove (out_buf, "ZLIB", 4);
	  memmove (out_buf + 4, &be64_size, sizeof (be64_size));
	}
      else
	{
	  Elf *elf = scn->elf;
	  ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

	  ElfW2(LIBELFBITS,Chdr) bchdr;
	  bchdr.ch_type = ELFCOMPRESS_ZLIB;
#if LIBELFBITS == 64
	  bchdr.ch_reserved = 0;
#endif
	  bchdr.ch_size = orig_size;
	  bchdr.ch_addralign = shdr->sh_addralign;
	  if (ehdr->e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (bchdr.ch_type);
	      CONVERT (bchdr.ch_size);
	      CONVERT (bchdr.ch_type);
	    }
	  memmove (out_buf, &bchdr, sizeof (ElfW2(LIBELFBITS,Chdr)));
	}

      /* This is the new raw data, replace and possibly free old data.  */
      scn->rawdata.d.d_buf = out_buf;
      scn->rawdata.d.d_type = ELF_T_BYTE;
      scn->rawdata.d.d_size = used;
      scn->rawdata.d.d_align = 1;

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr->sh_size = used;
      shdr->sh_addralign = 1;
      if (type == ELF_ZSCN_T_ELF)
	shdr->sh_flags |= SHF_COMPRESSED;

      /* The section is now compressed, but let the user call getchdr
	 to get it if they want.  */
      if ((void *) chdr != scn->rawdata_base)
        free (chdr);
      scn->chdr.ELFW(e,LIBELFBITS) = NULL;

      /* Existing existing data is no longer valid.  */
      scn->data_list_rear = NULL;
      if (scn->data_base != scn->rawdata_base)
	free (scn->data_base);
      scn->data_base = NULL;
      if (scn->elf->map_address == NULL
	  || scn->rawdata_base == scn->zdata_base)
	free (scn->rawdata_base);
      scn->zdata_base = NULL;
      scn->rawdata_base = out_buf;

      return shdr;
    }
  else
    {
      if (type != ELF_ZSCN_T_NONE)
	{
	  __libelf_seterrno (ELF_E_ALREADY_COMPRESSED);
	  return NULL;
	}

      if ((compressed != ELF_ZSCN_T_GNU && compressed != ELF_ZSCN_T_ELF)
	  || chdr->ch_type != ELFCOMPRESS_ZLIB)
	{
	  __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
	  return NULL;
	}

      Elf_Data *rawdata = elf_rawdata (scn, NULL);
      if (rawdata == NULL)
	return NULL;

      size_t size = chdr->ch_size;
      size_t hsize = (compressed == ELF_ZSCN_T_GNU
		      ? 4 + 8 /* GNU "ZLIB" + 8 byte size.  */
		      : sizeof (ElfW2(LIBELFBITS,Chdr)));

      void *buf_out = malloc (size);
      if (unlikely (buf_out == NULL))
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  return NULL;
	}

      /* rawdata_base must be non-NULL since we found a compressed header.  */
      void *buf_in = rawdata->d_buf + hsize;

      z_stream z =
	{
	  .next_in = buf_in,
	  .avail_in = rawdata->d_size - hsize,
	  .next_out = buf_out,
	  .avail_out = size
	};
      int zrc = inflateInit (&z);
      while (z.avail_in > 0 && likely (zrc == Z_OK))
	{
	  z.next_out = buf_out + (size - z.avail_out);
	  zrc = inflate (&z, Z_FINISH);
	  if (unlikely (zrc != Z_STREAM_END))
	    {
	      zrc = Z_DATA_ERROR;
	      break;
	    }
	  zrc = inflateReset (&z);
	}
      if (likely (zrc == Z_OK))
	zrc = inflateEnd (&z);

      if (unlikely (zrc != Z_OK) || unlikely (z.avail_out != 0))
	{
	  free (buf_out);
	  __libelf_seterrno (ELF_E_DECOMPRESS_ERROR);
	  return NULL;
	}

      /* This is the new raw data, replace and possibly free old data.  */
      scn->rawdata.d.d_buf = buf_out;
      scn->rawdata.d.d_type = __libelf_data_type (scn->elf, shdr->sh_type);
      scn->rawdata.d.d_size = size;
      scn->rawdata.d.d_align = chdr->ch_addralign;

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr->sh_size = size;
      shdr->sh_addralign = chdr->ch_addralign;
      shdr->sh_flags &= ~SHF_COMPRESSED;

      /* The section isn't compressed anymore.  */
      if ((void *) chdr != scn->rawdata_base)
        free (chdr);
      scn->chdr.ELFW(e,LIBELFBITS) = (void *) -1;
      scn->chdr_type = ELF_ZSCN_T_NONE;

      /* The (converted) data list must be reloaded, existing data is
	 no longer valid.  */
      scn->data_list_rear = NULL;
      if (scn->data_base != scn->rawdata_base)
	free (scn->data_base);
      scn->data_base = NULL;
      if (scn->elf->map_address == NULL
	  || scn->rawdata_base == scn->zdata_base)
	free (scn->rawdata_base);

      scn->rawdata_base = scn->zdata_base = buf_out;

      return shdr;
    }
}
