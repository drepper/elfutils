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

#ifndef DWARFLINT_CHECKED_READ_HH
#define DWARFLINT_CHECKED_READ_HH

#include "readctx.hh"
#include "locus.hh"
#include "dwarf_version.hh"

enum error_code
  {
    err_ok,     ///< The operation passed.
    err_fatal,  ///< The operation ended in unrecoverable error.
    err_nohl,   ///< There was an error, but low-level checks may continue.
  };

bool read_size_extra (read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		      int *offset_sizep, locus const &loc);

/// Read address size and return it via address_sizep and return 0.
/// Address size may be 4 or 8; for other values it's set depending or
/// addr_64, and err_nohl is returned.
error_code read_address_size (read_ctx *ctx, bool addr_64,
			      int *address_sizep, locus const &loc);

bool checked_read_uleb128 (read_ctx *ctx, uint64_t *ret,
			   locus const &loc, const char *what);

bool checked_read_sleb128 (read_ctx *ctx, int64_t *ret,
			   locus const &loc, const char *what);

bool checked_read_leb128 (read_ctx *ctx, form_width_t width, uint64_t *ret,
			  locus const &loc, const char *what);

/// Read value depending on the form width and storage class.
bool read_sc_value (uint64_t *valuep, form_width_t width,
		    read_ctx *ctx, locus const &loc);

/// Read value depending on the form width and storage class.
/// Value is returned via VALUEP, if that is non-NULL; for block
/// forms, the value is block length.  Block context is returned via
/// BLOCKP, in non-NULL; for string class, the block is the string
/// itself.
bool read_generic_value (read_ctx *ctx,
			 form_width_t width, storage_class_t storclass,
			 locus const &loc, uint64_t *valuep,
			 read_ctx *blockp);

#endif//DWARFLINT_CHECKED_READ_HH
