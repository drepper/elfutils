/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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
#include <vector>

#include "where.h"

/* Functions and data structures for address record handling.  We
   use that to check that all DIE references actually point to an
   existing die, not somewhere mid-DIE, where it just happens to be
   interpretable as a DIE.  */

struct addr_record
  : private std::vector<uint64_t>
{
  typedef std::vector<uint64_t> _super_t;
  size_t find (uint64_t addr) const;

public:
  bool has_addr (uint64_t addr) const;
  void add (uint64_t addr);
};

/* Functions and data structures for reference handling.  Just like
   the above, we use this to check validity of DIE references.
   Unlike the above, this is not stored as sorted set, but simply as
   an array of records, because duplicates are unlikely.  */

struct ref
{
  uint64_t addr; // Referee address
  locus *who;    // Referrer

  ref ();
  ref (uint64_t a_addr, locus const &a_who);
  ref (ref const &copy);
  ~ref ();
  ref &operator= (ref const &copy);
};

class ref_record
  : private std::vector<struct ref>
{
  typedef std::vector<struct ref> _super_t;
public:
  using _super_t::const_iterator;
  using _super_t::begin;
  using _super_t::end;
  using _super_t::push_back;
};

#endif//DWARFLINT_ADDR_RECORD_H
