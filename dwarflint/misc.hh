/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

#ifndef DWARFLINT_MISC_HH
#define DWARFLINT_MISC_HH

#include <cstring>
#include "locus.hh"

extern "C"
{
#include "../lib/libeu.h"
}

#define REALLOC(A, BUF)					\
  do {							\
    typeof ((A)) _a = (A);				\
    if (_a->size == _a->alloc)				\
      {							\
	if (_a->alloc == 0)				\
	  _a->alloc = 8;				\
	else						\
	  _a->alloc *= 2;				\
	_a->BUF = (typeof (_a->BUF))			\
	  xrealloc (_a->BUF,				\
		    sizeof (*_a->BUF) * _a->alloc);	\
      }							\
  } while (0)

bool address_aligned (uint64_t addr, uint64_t align);
bool necessary_alignment (uint64_t start, uint64_t length,
			  uint64_t align);

bool supported_version (unsigned version,
			size_t num_supported, locus const &loc, ...);

#define UNREACHABLE assert (!"unreachable")


#endif//DWARFLINT_MISC_HH
