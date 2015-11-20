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
#include <sys/param.h>
#include <unistd.h>
#include <zlib.h>

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Given a section, uses the (in-memory) Elf_Data to extract the
   original data size (including the given header size) and data
   alignment.  Returns a buffer that has at least hsize bytes (for the
   caller to fill in with a header) plus zlib compressed date.  Also
   returns the new buffer size in new_size (hsize + compressed data
   size).  */
void *
internal_function
__libelf_compress (Elf_Scn *scn, size_t hsize, int ei_data,
		   size_t *orig_size, size_t *orig_addralign,
		   size_t *new_size)
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

  *orig_addralign = data->d_align;
  *orig_size = data->d_size;

  /* Guess an output size. 1/8th of the original Elf_Data or at least 8K.  */
  size_t block = MAX (data->d_size / 8, 8 * 1024);
  size_t out_size = block;
  void *out_buf = malloc (out_size);
  if (out_buf == NULL)
    {
      __libelf_seterrno (ELF_E_NOMEM);
      return NULL;
    }

  /* Caller gets to fill in the header at the start.  Just skip it here.  */
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

  Elf_Data cdata;
  cdata.d_buf = NULL;

  /* Cleanup and NULL return on error.  Don't leak memory.  */
  void *deflate_error (int err)
  {
    __libelf_seterrno (err);
    deflateEnd (&z);
    free (out_buf);
    if (ei_data != MY_ELFDATA)
      free (cdata.d_buf);
    return NULL;
  }

  /* Loop over data buffers.  */
  int flush = Z_NO_FLUSH;
  do
    {
      /* Convert to raw if different endianess.  */
      cdata = *data;
      if (ei_data != MY_ELFDATA)
	{
	  /* Don't do this conversion in place, we might want to keep
	     the original data around, caller decides.  */
	  cdata.d_buf = malloc (data->d_size);
	  if (cdata.d_buf == NULL)
	    return deflate_error (ELF_E_NOMEM);
	  if (gelf_xlatetof (scn->elf, &cdata, data, ei_data) == NULL)
	    return deflate_error;
	}

      z.avail_in = cdata.d_size;
      z.next_in = cdata.d_buf;

      /* Get next buffer to see if this is the last one.  */
      data = elf_getdata (scn, data);
      if (data != NULL)
	{
	  *orig_addralign = MAX (*orig_addralign, data->d_align);
	  *orig_size += data->d_size;
	}
      else
	flush = Z_FINISH;

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

      if (ei_data != MY_ELFDATA)
	{
	  free (cdata.d_buf);
	  cdata.d_buf = NULL;
	}
    }
  while (flush != Z_FINISH); /* More data blocks.  */

  zrc = deflateEnd (&z);
  if (zrc != Z_OK)
    return deflate_error (ELF_E_COMPRESS_ERROR);

  *new_size = used;
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
__libelf_reset_rawdata (Elf_Scn *scn, void *buf, size_t size, size_t align,
			Elf_Type type)
{
  /* This is the new raw data, replace and possibly free old data.  */
  // XXX d_off, d_version, ...
  scn->rawdata.d.d_buf = buf;
  scn->rawdata.d.d_size = size;
  scn->rawdata.d.d_align = align;
  scn->rawdata.d.d_type = type;

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
    return -1;

  int elfclass = elf->class;
  int elfdata = ehdr.e_ident[EI_DATA];

  GElf_Shdr shdr;
  if (gelf_getshdr (scn, &shdr) == NULL)
    return -1;

  int compressed = (shdr.sh_flags & SHF_COMPRESSED);
  if (type == ELFCOMPRESS_ZLIB)
    {
      /* Compress/Deflate.  */
      if (compressed == 1)
	{
	  __libelf_seterrno (ELF_E_ALREADY_COMPRESSED);
	  return -1;
	}

      size_t hsize = (elfclass == ELFCLASS32
		      ? sizeof (Elf32_Chdr) : sizeof (Elf64_Chdr));
      size_t orig_size, orig_addralign, new_size;
      void *out_buf = __libelf_compress (scn, hsize, elfdata,
					 &orig_size, &orig_addralign,
					 &new_size);
      if (out_buf == NULL)
	return -1;

      /* Put the header in front of the data.  */
      if (elfclass == ELFCLASS32)
	{
	  Elf32_Chdr chdr;
	  chdr.ch_type = ELFCOMPRESS_ZLIB;
	  chdr.ch_size = orig_size;
	  chdr.ch_addralign = orig_addralign;
	  if (elfdata != MY_ELFDATA)
	    {
	      CONVERT (chdr.ch_type);
	      CONVERT (chdr.ch_size);
	      CONVERT (chdr.ch_addralign);
	    }
	  memcpy (out_buf, &chdr, sizeof (Elf32_Chdr));
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
	  memcpy (out_buf, &chdr, sizeof (Elf64_Chdr));
	}

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr.sh_size = new_size;
      shdr.sh_addralign = 1;
      shdr.sh_flags |= SHF_COMPRESSED;
      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, out_buf, new_size, 1, ELF_T_CHDR);

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

      GElf_Chdr chdr;
      if (gelf_getchdr (scn, &chdr) == NULL)
	return -1;

      if (chdr.ch_type != ELFCOMPRESS_ZLIB)
	{
	  __libelf_seterrno (ELF_E_UNKNOWN_COMPRESSION_TYPE);
	  return -1;
	}

      if (! powerof2 (chdr.ch_addralign))
	{
	  __libelf_seterrno (ELF_E_INVALID_ALIGN);
	  return -1;
	}

      /* Take the in-memory representation, so we can even handle a
	 section that has just been constructed (maybe it was copied
	 over from some other ELF file first with elf_newdata).  This
	 is slightly inefficient when the raw data needs to be
	 converted since then we'll be converting the whole buffer and
	 not just Chdr.  */
      Elf_Data *data = elf_getdata (scn, NULL);
      if (data == NULL)
	return -1;

      size_t hsize = (elfclass == ELFCLASS32
		      ? sizeof (Elf32_Chdr) : sizeof (Elf64_Chdr));
      size_t size_in = data->d_size - hsize;
      void *buf_in = data->d_buf + hsize;
      void *buf_out = __libelf_decompress (buf_in, size_in, chdr.ch_size);
      if (buf_out == NULL)
	return -1;

      /* Note we keep the sh_entsize as is, we assume it is setup
	 correctly and ignored when SHF_COMPRESSED is set.  */
      shdr.sh_size = chdr.ch_size;
      shdr.sh_addralign = chdr.ch_addralign;
      shdr.sh_flags &= ~SHF_COMPRESSED;
      // XXX Don't! this sets dirty flag...
      gelf_update_shdr (scn, &shdr);

      __libelf_reset_rawdata (scn, buf_out, chdr.ch_size, chdr.ch_addralign,
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
