/* x86_64 specific symbolic name handling.
   Copyright (C) 2002-2010 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <elf.h>
#include <stddef.h>

#define BACKEND		x86_64_
#include "libebl_CPU.h"


/* Check for the simple reloc types.  */
int
x86_64_reloc_simple_types (Ebl *ebl __attribute__ ((unused)),
			   const int **rel8_types, const int **rel4_types)
{
  static const int rel8[] = { R_X86_64_64, 0 };
  static const int rel4[] = { R_X86_64_32, R_X86_64_32S, 0 };
  *rel8_types = rel8;
  *rel4_types = rel4;
  return 0;
}
