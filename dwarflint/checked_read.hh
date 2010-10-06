/* Pedantic checking of DWARF files
   Copyright (C) 2010 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifndef DWARFLINT_CHECKED_READ_HH
#define DWARFLINT_CHECKED_READ_HH

#include "readctx.h"
#include "where.h"
#include "tables.hh"

bool read_size_extra (read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		      int *offset_sizep, where *where);

bool read_address_size (read_ctx *ctx,
			bool addr_64,
			int *address_sizep,
			where const *where);

bool checked_read_uleb128 (read_ctx *ctx, uint64_t *ret,
			   where *where, const char *what);

bool checked_read_sleb128 (read_ctx *ctx, int64_t *ret,
			   where *where, const char *what);

bool checked_read_leb128 (read_ctx *ctx, form_width_t width, uint64_t *ret,
			  where *where, const char *what);

#endif//DWARFLINT_CHECKED_READ_HH
