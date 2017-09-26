/* Pedantic checking of DWARF files
   Copyright (C) 2010, 2011 Red Hat, Inc.
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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>
#include <cassert>

#include "checked_read.hh"
#include "messages.hh"
#include "misc.hh"

bool
read_size_extra (struct read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		 int *offset_sizep, locus const &loc)
{
  if (size32 == DWARF3_LENGTH_64_BIT)
    {
      if (!read_ctx_read_8ubyte (ctx, sizep))
	{
	  wr_error (loc) << "can't read 64bit CU length.\n";
	  return false;
	}

      *offset_sizep = 8;
    }
  else if (size32 >= DWARF3_LENGTH_MIN_ESCAPE_CODE)
    {
      wr_error (loc)
	<< "unrecognized CU length escape value: " << size32 << ".\n";
      return false;
    }
  else
    {
      *sizep = size32;
      *offset_sizep = 4;
    }

  return true;
}

error_code
read_address_size (struct read_ctx *ctx, bool addr_64,
		   int *address_sizep, locus const &loc)
{
  uint8_t address_size;
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      wr_error (loc) << "can't read address size.\n";
      return err_fatal;
    }

  error_code ret = err_ok;
  if (address_size != 4 && address_size != 8)
    {
      /* Keep going.  Deduce the address size from ELF header, and try
	 to parse it anyway.  */
      wr_error (loc) << "invalid address size: " << (int)address_size
		     << " (only 4 or 8 allowed).\n";
      address_size = addr_64 ? 8 : 4;
      ret = err_nohl;
    }
  else if ((address_size == 8) != addr_64)
    {
      /* Keep going, we may still be able to parse it.  */
      wr_error (loc) << "CU reports address size of " << address_size
		     << " in " << (addr_64 ? 64 : 32) << "-bit ELF.\n";
      ret = err_nohl;
    }

  *address_sizep = address_size;
  return ret;
}

bool
checked_read_uleb128 (read_ctx *ctx, uint64_t *ret,
		      locus const &loc, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_uleb128 (ctx, ret);
  if (st < 0)
    wr_error (loc) << "can't read " << what << ".\n";
  else if (st > 0)
    {
      char buf[19]; // 16 hexa digits, "0x", terminating zero
      sprintf (buf, "%#" PRIx64, *ret);
      wr_format_leb128_message (loc, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

bool
checked_read_sleb128 (read_ctx *ctx, int64_t *ret,
		      locus const &loc, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_sleb128 (ctx, ret);
  if (st < 0)
    wr_error (loc) << "can't read " << what << ".\n";
  else if (st > 0)
    {
      char buf[20]; // sign, "0x", 16 hexa digits, terminating zero
      int64_t val = *ret;
      sprintf (buf, "%s%#" PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
      wr_format_leb128_message (loc, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

bool
checked_read_leb128 (read_ctx *ctx, form_width_t width, uint64_t *ret,
		     locus const &loc, const char *what)
{
  assert (width == fw_sleb || width == fw_uleb);
  if (width == fw_sleb)
    {
      int64_t svalue;
      if (!checked_read_sleb128 (ctx, &svalue, loc, what))
	return false;
      *ret = (uint64_t) svalue;
      return true;
    }
  else
    return checked_read_uleb128 (ctx, ret, loc, what);
}

bool
read_sc_value (uint64_t *valuep, form_width_t width,
	       read_ctx *ctx, locus const &loc)
{
  switch (width)
    {
    case fw_0:
      *valuep = 1;
      return true;

    case fw_1:
    case fw_2:
    case fw_4:
    case fw_8:
      return read_ctx_read_var (ctx, width, valuep);

    case fw_uleb:
    case fw_sleb:
      return checked_read_leb128 (ctx, width, valuep,
				  loc, "attribute value");

    case fw_unknown:
      ;
    }
  UNREACHABLE;
}

bool
read_generic_value (read_ctx *ctx,
		    form_width_t width, storage_class_t storclass,
		    locus const &loc, uint64_t *valuep, read_ctx *blockp)
{
  uint64_t value;
  if (storclass == sc_value
      || storclass == sc_block)
    {
      if (!read_sc_value (&value, width, ctx, loc))
	return false;
      if (valuep != NULL)
	*valuep = value;
      if (storclass == sc_value)
	return true;
    }

  unsigned char const *start = ctx->ptr;
  if (storclass == sc_string)
    {
      if (!read_ctx_read_str (ctx))
	return false;
    }
  else if (storclass == sc_block)
    {
      if (!read_ctx_skip (ctx, value))
	return false;
    }

  if (blockp != NULL)
    {
      if (!read_ctx_init_sub (blockp, ctx, start, ctx->ptr))
	return false;
    }

  return true;
}
