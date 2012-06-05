/* S/390-specific symbolic name handling.
   Copyright (C) 2005-2010 Red Hat, Inc.
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

#include <elf.h>
#include <stddef.h>

#define BACKEND		s390_
#include "libebl_CPU.h"

/* Check for the simple reloc types.  */
int
s390_reloc_simple_types (Ebl *ebl __attribute__ ((unused)),
			 const int **rel8_types, const int **rel4_types)
{
  static const int rel8[] = { R_390_64, 0 };
  static const int rel4[] = { R_390_32, 0 };
  *rel8_types = rel8;
  *rel4_types = rel4;
  return 0;
}
