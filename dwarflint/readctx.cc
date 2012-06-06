/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2009.

   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "readctx.hh"
#include "../libdw/dwarf.h"

#include <stdlib.h>
#include <assert.h>
#include <byteswap.h>
#include <inttypes.h>

/* read_Xubyte_* is basically cut'n'paste from memory-access.h.  */
union unaligned
  {
    void *p;
    uint16_t u2;
    uint32_t u4;
    uint64_t u8;
    int16_t s2;
    int32_t s4;
    int64_t s8;
  } __attribute__ ((packed));

static uint16_t
read_2ubyte_unaligned (const void *p, bool other_byte_order)
{
  const union unaligned *up = (const union unaligned *)p;
  if (other_byte_order)
    return bswap_16 (up->u2);
  return up->u2;
}

/* Prefix with dwarflint_ for export, so that it doesn't get confused
   with functions and macros in memory-access.h.  */
uint32_t
dwarflint_read_4ubyte_unaligned (const void *p, bool other_byte_order)
{
  const union unaligned *up = (const union unaligned *)p;
  if (other_byte_order)
    return bswap_32 (up->u4);
  return up->u4;
}

uint64_t
dwarflint_read_8ubyte_unaligned (const void *p, bool other_byte_order)
{
  const union unaligned *up = (const union unaligned *)p;
  if (other_byte_order)
    return bswap_64 (up->u8);
  return up->u8;
}


#define read_2ubyte_unaligned_inc(Addr, OtherByteOrder)			\
  ({ uint16_t t_ = read_2ubyte_unaligned (Addr, OtherByteOrder);	\
    Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 2);		\
    t_; })

#define read_4ubyte_unaligned_inc(Addr, OtherByteOrder)			\
  ({ uint32_t t_ = dwarflint_read_4ubyte_unaligned (Addr, OtherByteOrder); \
    Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 4);		\
    t_; })

#define read_8ubyte_unaligned_inc(Addr, OtherByteOrder)			\
  ({ uint64_t t_ = dwarflint_read_8ubyte_unaligned (Addr, OtherByteOrder); \
    Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 8);		\
    t_; })



void
read_ctx_init (struct read_ctx *ctx, Elf_Data *data, bool other_byte_order)
{
  if (data == NULL)
    abort ();

  ctx->data = data;
  ctx->begin = (const unsigned char *)data->d_buf;
  ctx->end = (const unsigned char *)data->d_buf + data->d_size;
  ctx->ptr = (const unsigned char *)data->d_buf;
  ctx->other_byte_order = other_byte_order;
}

bool
read_ctx_init_sub (struct read_ctx *ctx, struct read_ctx *parent,
		   const unsigned char *begin, const unsigned char *end)
{
  if (parent == NULL)
    abort ();

  if (begin < parent->begin
      || end > parent->end)
    return false;

  ctx->data = parent->data;
  ctx->begin = begin;
  ctx->end = end;
  ctx->ptr = begin;
  ctx->other_byte_order = parent->other_byte_order;
  return true;
}

uint64_t
read_ctx_get_offset (struct read_ctx *ctx)
{
  assert (ctx->ptr >= ctx->begin);
  return (uint64_t)(ctx->ptr - ctx->begin);
}

bool
read_ctx_need_data (struct read_ctx *ctx, size_t length)
{
  const unsigned char *ptr = ctx->ptr + length;
  return ptr <= ctx->end && (length == 0 || ptr > ctx->ptr);
}

bool
read_ctx_read_ubyte (struct read_ctx *ctx, unsigned char *ret)
{
  if (!read_ctx_need_data (ctx, 1))
    return false;
  if (ret != NULL)
    *ret = *ctx->ptr;
  ctx->ptr++;
  return true;
}

int
read_ctx_read_uleb128 (struct read_ctx *ctx, uint64_t *ret)
{
  uint64_t result = 0;
  int shift = 0;
  int size = 8 * sizeof (result);
  bool zero_tail = false;

  while (1)
    {
      uint8_t byte;
      if (!read_ctx_read_ubyte (ctx, &byte))
	return -1;

      uint8_t payload = byte & 0x7f;
      zero_tail = payload == 0 && shift > 0;
      result |= (uint64_t)payload << shift;
      shift += 7;
      if (shift > size && byte != 0x1)
	return -1;
      if ((byte & 0x80) == 0)
	break;
    }

  if (ret != NULL)
    *ret = result;
  return zero_tail ? 1 : 0;
}

int
read_ctx_read_sleb128 (struct read_ctx *ctx, int64_t *ret)
{
  int64_t result = 0;
  int shift = 0;
  int size = 8 * sizeof (result);
  bool zero_tail = false;
  bool sign = false;

  while (1)
    {
      uint8_t byte;
      if (!read_ctx_read_ubyte (ctx, &byte))
	return -1;

      uint8_t payload = byte & 0x7f;
      zero_tail = shift > 0 && ((payload == 0x7f && sign)
				|| (payload == 0 && !sign));
      sign = (byte & 0x40) != 0; /* Set sign for rest of loop & next round.  */
      result |= (int64_t)payload << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	{
	  if (shift < size && sign)
	    result |= -((int64_t)1 << shift);
	  break;
	}
      if (shift > size)
	return -1;
    }

  if (ret != NULL)
    *ret = result;
  return zero_tail ? 1 : 0;
}

bool
read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret)
{
  if (!read_ctx_need_data (ctx, 2))
    return false;
  uint16_t val = read_2ubyte_unaligned_inc (ctx->ptr, ctx->other_byte_order);
  if (ret != NULL)
    *ret = val;
  return true;
}

bool
read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret)
{
  if (!read_ctx_need_data (ctx, 4))
    return false;
  uint32_t val = read_4ubyte_unaligned_inc (ctx->ptr, ctx->other_byte_order);
  if (ret != NULL)
    *ret = val;
  return true;
}

bool
read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret)
{
  if (!read_ctx_need_data (ctx, 8))
    return false;
  uint64_t val = read_8ubyte_unaligned_inc (ctx->ptr, ctx->other_byte_order);
  if (ret != NULL)
    *ret = val;
  return true;
}

bool
read_ctx_read_offset (struct read_ctx *ctx, bool dwarf64, uint64_t *ret)
{
  if (dwarf64)
    return read_ctx_read_8ubyte (ctx, ret);

  uint32_t v;
  if (!read_ctx_read_4ubyte (ctx, &v))
    return false;

  if (ret != NULL)
    *ret = (uint64_t)v;
  return true;
}

bool
read_ctx_read_var (struct read_ctx *ctx, int width, uint64_t *ret)
{
  switch (width)
    {
    case 4:
    case 8:
      return read_ctx_read_offset (ctx, width == 8, ret);
    case 2:
      {
	uint16_t val;
	if (!read_ctx_read_2ubyte (ctx, &val))
	  return false;
	*ret = val;
	return true;
      }
    case 1:
      {
	uint8_t val;
	if (!read_ctx_read_ubyte (ctx, &val))
	  return false;
	*ret = val;
	return true;
      }
    default:
      return false;
    };
}

const char *
read_ctx_read_str (struct read_ctx *ctx)
{
  const char *ret = (const char *)ctx->ptr;
  uint8_t byte;
  do
    if (!read_ctx_read_ubyte (ctx, &byte))
      return NULL;
  while (byte != 0);
  return ret;
}

bool
read_ctx_skip (struct read_ctx *ctx, uint64_t len)
{
  if (!read_ctx_need_data (ctx, len))
    return false;
  ctx->ptr += len;
  return true;
}

bool
read_ctx_eof (struct read_ctx *ctx)
{
  return !read_ctx_need_data (ctx, 1);
}

static void
update_off (struct read_ctx *ctx,
	    uint64_t *ret_off)
{
  if (ret_off != NULL)
    *ret_off = (uint64_t)(ctx->ptr - ctx->begin);
}

bool
read_check_zero_padding (struct read_ctx *ctx,
			 uint64_t *ret_off_start,
			 uint64_t *ret_off_end)
{
  assert (ctx->ptr != ctx->end);
  update_off (ctx, ret_off_start);
  bool ret = true;

  const unsigned char *save_ptr = ctx->ptr;
  while (!read_ctx_eof (ctx))
    if (*ctx->ptr++ != 0)
      {
	ctx->ptr = save_ptr;
	goto done;
      }

 done:
  update_off (ctx, ret_off_end);
  return ret;
}
