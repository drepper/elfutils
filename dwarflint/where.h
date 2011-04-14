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

#ifndef DWARFLINT_WHERE_H
#define DWARFLINT_WHERE_H

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

  virtual locus const *next () const
  {
    return NULL;
  }

  virtual ~locus () {}
};

struct section_locus
  : public locus
{
  section_id _m_sec;
  uint64_t _m_offset;

public:
  explicit section_locus (section_id sec, uint64_t offset = -1)
    : _m_sec (sec)
    , _m_offset (offset)
  {}

  section_locus (section_locus const &copy)
    : _m_sec (copy._m_sec)
    , _m_offset (copy._m_offset)
  {}

  std::string format (bool brief = false) const;

  locus *
  clone () const
  {
    return new section_locus (*this);
  }
};

struct where
  : public locus
{
  class formatter
  {
  public:
    virtual ~formatter () {}
    virtual std::string format (where const &wh, bool brief = false) const = 0;
  };

  class simple_formatter;

private:
  formatter const *_m_formatter;

  uint64_t _m_addr1; // E.g. a CU offset.
  uint64_t _m_addr2; // E.g. a DIE address.
  uint64_t _m_addr3; // E.g. an attribute.

public:
  locus const *ref; // Related reference, e.g. an abbrev related to
		    // given DIE.
  locus const *_m_next; // For forming "caused-by" chains.

  explicit where (formatter const *fmt = NULL,
		  locus const *nxt = NULL);

  where &operator= (where const &copy);

  std::string format (bool brief = false) const;

  locus const *
  next () const
  {
    return _m_next;
  }

  locus *
  clone () const
  {
    return new where (*this);
  }

  void
  set_next (locus const *nxt)
  {
    assert (_m_next == NULL);
    _m_next = nxt;
  }

  void reset_1 (uint64_t addr);
  void reset_2 (uint64_t addr);
  void reset_3 (uint64_t addr);
};

inline void
where_reset_1 (struct where *wh, uint64_t addr)
{
  wh->reset_1 (addr);
}

inline void
where_reset_2 (struct where *wh, uint64_t addr)
{
  wh->reset_2 (addr);
}

inline void
where_reset_3 (struct where *wh, uint64_t addr)
{
  wh->reset_3 (addr);
}

where WHERE (section_id sec, locus const *next = NULL);

inline std::ostream &
operator << (std::ostream &os, locus const &loc)
{
  os << loc.format ();
  return os;
}

#endif//DWARFLINT_WHERE_H
