/* Copyright (C) 2012 Red Hat, Inc.
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

#include <stddef.h>

#include "known-dwarf.h"
#include "dwarf.h"

const char *
dwarf_locexpr_opcode_string (unsigned int code)
{
  static const char *const known[] =
    {
      /* Normally we can't affort building huge table of 64K entries,
	 most of them zero, just because there are a couple defined
	 values at the far end.  In case of opcodes, it's OK.  */
#define ONE_KNOWN_DW_OP_DESC(NAME, CODE, DESC) ONE_KNOWN_DW_OP(NAME, CODE)
#define ONE_KNOWN_DW_OP(NAME, CODE) [CODE] = #NAME,
      ALL_KNOWN_DW_OP
#undef ONE_KNOWN_DW_OP
#undef ONE_KNOWN_DW_OP_DESC
    };

  const char *ret = NULL;
  if (likely (code < sizeof (known) / sizeof (known[0])))
    ret = known[code];

  return ret;
}
