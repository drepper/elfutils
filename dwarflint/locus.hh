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

#ifndef DWARFLINT_WHERE_HH
#define DWARFLINT_WHERE_HH

#include "section_id.hh"

#include <stdint.h>
#include <stdlib.h>
#include <iosfwd>
#include <iostream>
#include <cassert>

class locus
{
public:
  virtual std::string format (bool brief = false) const = 0;
  virtual locus *clone () const = 0;
  virtual ~locus () {}
};

template <class T>
class clonable_locus
  : public locus
{
public:
  virtual locus *clone () const
  {
    return new T (*static_cast<T const*> (this));
  }
};

std::string format_simple_locus (char const *(*N) (),
				 void (*F) (std::ostream &, uint64_t),
				 bool brief,
				 section_id sec,
				 uint64_t off);

template<char const *(*N) (),
	 void (*F) (std::ostream &, uint64_t)>
class simple_locus
  : public clonable_locus<simple_locus<N, F> >
{
  section_id _m_sec;
  uint64_t _m_offset;

public:
  explicit simple_locus (section_id sec, uint64_t offset = -1)
    : _m_sec (sec)
    , _m_offset (offset)
  {}

  std::string format (bool brief = false) const
  {
    return format_simple_locus (N, F, brief, _m_sec, _m_offset);
  }
};

template<section_id S,
	 char const *(*N) (),
	 void (*F) (std::ostream &, uint64_t)>
class fixed_locus
  : public simple_locus<N, F>
{
public:
  explicit fixed_locus (uint64_t offset = -1)
    : simple_locus<N, F> (S, offset)
  {}
};

struct locus_simple_fmt {
  static char const *offset () { return "offset"; }

  static void hex (std::ostream &ss, uint64_t off) {
    ss << "0x" << std::hex << off;
  }
  static void dec (std::ostream &ss, uint64_t off) {
    ss << std::dec << off;
  }
};
typedef simple_locus<locus_simple_fmt::offset,
		     locus_simple_fmt::hex> section_locus;

inline std::ostream &
operator << (std::ostream &os, locus const &loc)
{
  os << loc.format ();
  return os;
}

#endif//DWARFLINT_WHERE_HH
