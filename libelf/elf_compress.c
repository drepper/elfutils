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

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void *
internal_function
__libelf_compress (Elf_Scn *scn, size_t hsize, int ei_data,
		   size_t orig_size, size_t *size)
{
  /* The compressed data is the on-disk data.  We simplify the
     implementation a bit by asking for the (converted) in-memory
     data (which might be all there is if the user created it with
     elf_newdata) and then convert back to raw if needed before
     compressing.  Should be made a bit more clever to directly
     use raw if that is directly available.  */
  Elf_Data *data = elf_getdata (scn, NULL);
  if (data == NULL)
    return NULL;

  /* Guess an output size. 1/8th of the original Elf_Data or at least 8K.  */
  size_t block = MAX (orig_size, data->d_size) / 8;
  if (block < 8 * 1024)
    block = 8 * 1024;
  size_t out_size = block;
  void *out_buf = malloc (out_size);
  if (out_buf == NULL)
    {
      __libelf_seterrno (ELF_E_NOMEM);
      return NULL;
    }

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
  do
    {
      /* Convert to raw if different endianess.  */
      if (ei_data != MY_ELFDATA)
	{
	  if (gelf_xlatetof (scn->elf, data, data, ei_data) == NULL)
	    return deflate_error;
	}
      z.avail_in = data->d_size;
      z.next_in = data->d_buf;
      /* Get next buffer to see if this is the last one.  */
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

  *size = used;
  return out_buf;
}

void *
internal_function
__libelf_decompress (void *buf_in, size_t size_in, size_t size_out)
{
  void *buf_out = malloc (size_out);
  if (unlikely (buf_out == NULL))
    {
      __libelf_seterrno (ELF_E_NOMEM);
      return NULL;
    }

  z_stream z =
    {
      .next_in = buf_in,
      .avail_in = size_in,
      .next_out = buf_out,
      .avail_out = size_out
    };
  int zrc = inflateInit (&z);
  while (z.avail_in > 0 && likely (zrc == Z_OK))
    {
      z.next_out = buf_out + (size_out - z.avail_out);
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

  return buf_out;
}

void
internal_function
__libelf_reset_rawdata (Elf_Scn *scn, void *buf, size_t size, size_t align)
{
  /* This is the new raw data, replace and possibly free old data.  */
  scn->rawdata.d.d_buf = buf;
  scn->rawdata.d.d_size = size;
  scn->rawdata.d.d_align = align;

  /* Existing existing data is no longer valid.  */
  scn->data_list_rear = NULL;
  if (scn->data_base != scn->rawdata_base)
    free (scn->data_base);
  scn->data_base = NULL;
  if (scn->elf->map_address == NULL
      || scn->rawdata_base == scn->zdata_base)
    free (scn->rawdata_base);

  scn->rawdata_base = buf;
}

int
elf_compress (Elf_Scn *scn, int type)
{
  if (scn == NULL)
    return -1;

  Elf *elf = scn->elf;
  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    return NULL;

  int elfclass = elf->class;
  int elfdata = ehdr.e_ident[EI_DATA];

  GElf_Shdr shdr;
  if (gelf_getshdr (scn, &shdr) == NULL)
    return NULL;

  int compressed = (shdr.sh_flags & SHF_COMPRESSED);
  if (type == ELFCOMPRESS_ZLIB)
    {
      /* Compress/Deflate.  */
      if (compressed == 1)
	{
	  __libelf_seterrno (ELF_E_ALREADY_COMPRESSED);
	  return -1;
	}

      // XXX sh_size vs Elf_Data sizes. likewise for addralign.
      size_t orig_size = shdr.sh_size;
      size_t hsize = (elfclass == ELFCLASS32
		      ? sizeof (Elf32_Chdr) : sizeof (Elf64_Chdr));
      size_t size;
      void *out_buf = __libelf_compress (scn, hsize, elfdata,
					 orig_size, &size);
      if (out_buf == NULL)
	return NULL;

      /* Put the header in front of the data.  */
      if (elfclass == ELFCLASS32)
	{
	  Elf32_Chdr chdr;
	  chdr.ch_type = ELFCOMPRESS_ZLIB;
	  chdr.ch_size = orig_size;
	  chdr.ch_addralign = shdr.sh_addralign;
	  if (elfdata != MY_ELFDATA)
	    {
	      CONVERT (chdr.ch_type);
	      CONVERT (chdr.ch_size);
	      CONVERT (chdr.ch_addralign);
	    }
	  memmove (out_buf, &chdr, sizeof (Elf32_Chdr));
	}
      else
	{
	  Elf64_Chdr chdr;
	  chdr.ch_type = ELFCOMPRESS_ZLIB;
	  chdr.ch_reserved = 0;
	  chdr.ch_size = orig_size;
	  chdr.ch_addralign = shdr.sh_addralign;
	  if (elfdata != MY_ELFDATA)
	    {
	      CONVERT (chdr.ch_type);
	      CONVERT (chdr.ch_reserved);
	      CONVERT (chdr.ch_size);
	      CONVERT (chdr.ch_addralign);
	    }
	  memmove (out_buf, &chdr, sizeof (Elf64_Chdr));
	}

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr.sh_size = size;
      shdr.sh_addralign = 1;
      shdr.sh_flags |= SHF_COMPRESSED;
      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, out_buf, size, 1);

      /* The section is now compressed, we could keep the uncompressed
	 data around, but since that might have been multiple Elf_Data
	 buffers let the user uncompress it explicitly again if they
	 want it to simplify bookkeeping.  */
      scn->zdata_base = NULL;

      return 0;
    }
  else if (type == 0)
    {
      /* Decompress/Inflate.  */
      if (compressed == 0)
	{
	  __libelf_seterrno (ELF_E_NOT_COMPRESSED);
	  return -1;
	}

      /* In theory the user could have constucted a compressed section
	 by hand.  But we always just take the rawdata directly and
	 decompress that.  */
      Elf_Data *data = elf_rawdata (scn, NULL);
      if (data == NULL)
	return -1;

      /* Extract the header data first.  */
      int ctype;
      size_t size;
      size_t align;
      size_t hsize;
      if (elfclass == ELFCLASS32)
	{
	  hsize = sizeof (Elf32_Chdr);
	  if (data->d_size < hsize)
	    {
	      __libelf_seterrno (ELF_E_INVALID_DATA);
	      return -1;
	    }

	  Elf32_Chdr chdr;
	  if (elfdata == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((uintptr_t) data->d_buf & (__alignof__ (Elf32_Chdr) - 1))
		      == 0)))
	    chdr = *(Elf32_Chdr *) data->d_buf;
	  else
	    memcpy (&chdr, data->d_buf, hsize);

	  if (elfdata == MY_ELFDATA)
	    {
	      ctype = chdr.ch_type;
	      size = chdr.ch_size;
	      align = chdr.ch_addralign;
	    }
	  else
	    {
	      CONVERT_TO (ctype, chdr.ch_type);
	      CONVERT_TO (size, chdr.ch_size);
	      CONVERT_TO (align, chdr.ch_addralign);
	    }
	}
      else
	{
	  hsize = sizeof (Elf64_Chdr);
	  if (data->d_size < hsize)
	    {
	      __libelf_seterrno (ELF_E_INVALID_DATA);
	      return -1;
	    }

	  Elf64_Chdr chdr;
	  if (elfdata == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((uintptr_t) data->d_buf & (__alignof__ (Elf64_Chdr) - 1))
		      == 0)))
	    chdr = *(Elf64_Chdr *) data->d_buf;
	  else
	    memcpy (&chdr, data->d_buf, hsize);

	  if (elfdata == MY_ELFDATA)
	    {
	      ctype = chdr.ch_type;
	      size = chdr.ch_size;
	      align = chdr.ch_addralign;
	    }
	  else
	    {
	      CONVERT_TO (ctype, chdr.ch_type);
	      CONVERT_TO (size, chdr.ch_size);
	      CONVERT_TO (align, chdr.ch_addralign);
	    }
	}

      if (ctype != ELFCOMPRESS_ZLIB)
	{
	  __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
	  return -1;
	}
      
      /* rawdata_base must be non-NULL since we found a compressed header.  */
      size_t size_in = data->d_size - hsize;
      void *buf_in = data->d_buf + hsize;
      void *buf_out = __libelf_decompress (buf_in, size_in, size);
      if (buf_out == NULL)
	return -1;

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr.sh_size = size;
      shdr.sh_addralign = align;
      shdr.sh_flags &= ~SHF_COMPRESSED;
      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, buf_out, size, align);

      scn->zdata_base = buf_out;

      return 0;
    }
  else
    {
      __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
      return -1;
    }
}
