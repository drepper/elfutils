/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

#ifndef DWARFLINT_ADDR_RECORD_H
#define DWARFLINT_ADDR_RECORD_H

#include <stdlib.h>
#include <stdint.h>
#include "where.h"
#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  /* Functions and data structures for address record handling.  We
     use that to check that all DIE references actually point to an
     existing die, not somewhere mid-DIE, where it just happens to be
     interpretable as a DIE.  */

  struct addr_record
  {
    size_t size;
    size_t alloc;
    uint64_t *addrs;
  };

  size_t addr_record_find_addr (struct addr_record *ar, uint64_t addr);
  bool addr_record_has_addr (struct addr_record *ar, uint64_t addr);
  void addr_record_add (struct addr_record *ar, uint64_t addr);
  void addr_record_free (struct addr_record *ar);

  /* Functions and data structures for reference handling.  Just like
     the above, we use this to check validity of DIE references.
     Unlike the above, this is not stored as sorted set, but simply as
     an array of records, because duplicates are unlikely.  */

  struct ref
  {
    uint64_t addr; // Referree address
    struct where who;  // Referrer
  };

  struct ref_record
  {
    size_t size;
    size_t alloc;
    struct ref *refs;
  };

  void ref_record_add (struct ref_record *rr, uint64_t addr,
		       struct where *referrer);
  void ref_record_free (struct ref_record *rr);

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_ADDR_RECORD_H
