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

#ifdef __cplusplus
#include <iosfwd>
extern "C"
{
#endif

  enum where_formatting
  {
    wf_plain = 0, /* Default formatting for given section.  */
    wf_cudie,
  };

  struct where
  {
    enum section_id section;
    enum where_formatting formatting;
    uint64_t addr1; // E.g. a CU offset.
    uint64_t addr2; // E.g. a DIE address.
    uint64_t addr3; // E.g. an attribute.
    struct where const *ref; // Related reference, e.g. an abbrev
			     // related to given DIE.
    struct where const *next; // For forming "caused-by" chains.
  };

  extern const char *where_fmt (const struct where *wh,	char *ptr);
  extern void where_fmt_chain (const struct where *wh, const char *severity);
  extern void where_reset_1 (struct where *wh, uint64_t addr);
  extern void where_reset_2 (struct where *wh, uint64_t addr);
  extern void where_reset_3 (struct where *wh, uint64_t addr);

#ifdef __cplusplus
}

#include <iostream>

inline where
WHERE (section_id sec, where const *next = NULL)
{
  where ret = {sec, wf_plain,
	       (uint64_t)-1,
	       (uint64_t)-1,
	       (uint64_t)-1,
	       NULL, next};
  return ret;
}


inline std::ostream &
operator << (std::ostream &os, where const &wh)
{
  os << where_fmt (&wh, NULL);
  return os;
}

#endif

#endif//DWARFLINT_WHERE_H
