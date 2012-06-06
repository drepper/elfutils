/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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

#ifndef DWARFLINT_ADDR_RECORD_H
#define DWARFLINT_ADDR_RECORD_H

#include <stdlib.h>
#include <stdint.h>
#include <vector>

#include "locus.hh"

/// Address record is used to check that all DIE references actually
/// point to an existing die, not somewhere mid-DIE, where it just
/// happens to be interpretable as a DIE.  This is stored as sorted
/// array for quick lookup and duplicate removal.
struct addr_record
  : private std::vector<uint64_t>
{
  typedef std::vector<uint64_t> _super_t;
  size_t find (uint64_t addr) const;

public:
  bool has_addr (uint64_t addr) const;
  void add (uint64_t addr);
};

/// One reference for use in ref_record, parametrized by locus type.
template <class L>
struct ref_T
{
  uint64_t addr; // Referee address
  L who;         // Referrer

  ref_T ()
    : addr (-1)
  {}

  ref_T (uint64_t a_addr, L const &a_who)
    : addr (a_addr)
    , who (a_who)
  {}
};

/// Reference record is used to check validity of DIE references.
/// Unlike the above, this is not stored as sorted set, but simply as
/// an array of records, because duplicates are unlikely.
template <class L>
class ref_record_T
  : private std::vector<ref_T<L> >
{
  typedef std::vector<ref_T<L> > _super_t;
public:
  using _super_t::const_iterator;
  using _super_t::begin;
  using _super_t::end;
  using _super_t::push_back;
};

#endif//DWARFLINT_ADDR_RECORD_H
