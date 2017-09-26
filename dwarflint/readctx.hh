/* Pedantic checking of DWARF files
   Copyright (C) 2009, 2010 Red Hat, Inc.
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

#ifndef dwarflint_readctx_h
#define dwarflint_readctx_h

#include <stdbool.h>
#include "../libelf/libelf.h"

/* Functions and data structures related to bounds-checked
   reading.  */

struct read_ctx
{
  Elf_Data *data;
  const unsigned char *ptr;
  const unsigned char *begin;
  const unsigned char *end;
  bool other_byte_order;
};

uint32_t dwarflint_read_4ubyte_unaligned (const void *p,
					  bool other_byte_order);
uint64_t dwarflint_read_8ubyte_unaligned (const void *p,
					  bool other_byte_order);


void read_ctx_init (struct read_ctx *ctx,
		    Elf_Data *data,
		    bool other_byte_order);
bool read_ctx_init_sub (struct read_ctx *ctx,
			struct read_ctx *parent,
			const unsigned char *begin,
			const unsigned char *end);
uint64_t read_ctx_get_offset (struct read_ctx *ctx);
bool read_ctx_need_data (struct read_ctx *ctx, size_t length);
bool read_ctx_read_ubyte (struct read_ctx *ctx, unsigned char *ret);
int read_ctx_read_uleb128 (struct read_ctx *ctx, uint64_t *ret);
int read_ctx_read_sleb128 (struct read_ctx *ctx, int64_t *ret);
bool read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret);
bool read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret);
bool read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret);
bool read_ctx_read_offset (struct read_ctx *ctx, bool dwarf64,
			   uint64_t *ret);
bool read_ctx_read_var (struct read_ctx *ctx, int width, uint64_t *ret);
const char *read_ctx_read_str (struct read_ctx *ctx);
bool read_ctx_skip (struct read_ctx *ctx, uint64_t len);
bool read_ctx_eof (struct read_ctx *ctx);

/* See if what remains in the read context is just a zero padding.  If
   yes, return true.  If it isn't, revert the read pointer back as if
   nothing had happened and return false.  Furthermore, in any case,
   if any of the ret pointers is non-NULL, it is filled, respectively,
   with start and end offset of the zero padding run.  */
bool read_check_zero_padding (struct read_ctx *ctx,
			      uint64_t *ret_off_start,
			      uint64_t *ret_off_end);

#endif
