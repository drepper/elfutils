/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009,2010,2011 Red Hat, Inc.
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

#include "where.h"
#include "section_id.hh"

#include <cinttypes>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <sstream>

class where::simple_formatter
  : public where::formatter
{
  char const *_m_name;
  char const *_m_fmt1;
  char const *_m_fmt2;
  char const *_m_fmt3;

public:
  simple_formatter (char const *name = NULL,
		    char const *fmt1 = NULL,
		    char const *fmt2 = NULL,
		    char const *fmt3 = NULL)
    : _m_name (name)
    , _m_fmt1 (fmt1)
    , _m_fmt2 (fmt2)
    , _m_fmt3 (fmt3)
  {}

  virtual std::string
  format (where const &wh, bool brief) const
  {
    std::string ret;

    brief = brief || _m_name == NULL;
    if (!brief)
      ret += _m_name;

    char const *const *formats = &_m_fmt1;
    uint64_t const *addrs = &wh._m_addr1;

    for (size_t i = 3; i > 0; --i)
      {
	size_t idx = i - 1;
	if (addrs[idx] == (uint64_t)-1)
	  continue;

	assert (formats[idx] != NULL);

	/* GCC insists on checking format parameters and emits a warning
	   when we don't use string literal.  With -Werror this ends up
	   being hard error.  So instead we walk around this warning by
	   using function pointer.  */
	int (*x_asprintf)(char **strp, const char *fmt, ...) = asprintf;

	if (!brief)
	  ret += ": ";
	char *buf;
	if (x_asprintf (&buf, formats[idx], addrs[idx]) >= 0)
	  {
	    ret += buf;
	    free (buf);
	  }
	else
	  ret += formats[idx];

	break;
      }

    if (wh.ref != NULL)
      {
	ret += " (";
	ret += wh.ref->format (true);
	ret += ')';
      }

    return ret;
  }
};

namespace
{
  template<size_t size>
  class simple_formatters
    : public std::array<where::simple_formatter, size>
  {
  protected:
    void
    add (section_id id, char const *name,
	 char const *addr1f = NULL,
	 char const *addr2f = NULL,
	 char const *addr3f = NULL)
    {
      (*this)[id] = where::simple_formatter (name, addr1f, addr2f, addr3f);
    }
  };

  class section_formatters
    : public simple_formatters<count_debuginfo_sections>
  {
  public:
    section_formatters ()
    {
      add (sec_aranges, ".debug_aranges",
	   "table %"PRId64, "arange %#"PRIx64);

      add (sec_pubnames, ".debug_pubnames",
	   "pubname table %"PRId64, "pubname %#"PRIx64);

      add (sec_pubtypes, ".debug_pubtypes",
	   "pubtype table %"PRId64, "pubtype %#"PRIx64);

      add (sec_line, ".debug_line", "table %"PRId64, "offset %#"PRIx64);

      add (sec_loc, ".debug_loc", "loclist %#"PRIx64, "offset %#"PRIx64);

      add (sec_mac, ".debug_mac");

      add (sec_ranges, ".debug_ranges", "rangelist %#"PRIx64, "offset %#"PRIx64);
    }
  } const section_fmts;
}

namespace
{
  where::formatter const *
  wf_for_section (section_id sec)
  {
    return &section_fmts[sec];
  }
}

std::string
section_locus::format (bool) const
{
  std::stringstream ss;
  ss << section_name[_m_sec];
  if (_m_offset != (uint64_t)-1)
    ss << ", offset 0x" << std::hex << _m_offset;
  return ss.str ();
}

where
WHERE (section_id sec, locus const *next)
{
  assert (sec != sec_abbrev);
  assert (sec != sec_str);
  assert (sec != sec_info);
  where::formatter const *fmt = wf_for_section (sec);
  return where (fmt, next);
}

where::where (formatter const *fmt, locus const *nxt)
  : _m_formatter (fmt)
  , _m_addr1 ((uint64_t)-1)
  , _m_addr2 ((uint64_t)-1)
  , _m_addr3 ((uint64_t)-1)
  , ref (NULL)
  , _m_next (nxt)
{
}

where &
where::operator= (where const &copy)
{
  _m_formatter = copy._m_formatter;

  _m_addr1 = copy._m_addr1;
  _m_addr2 = copy._m_addr2;
  _m_addr3 = copy._m_addr3;

  ref = copy.ref;
  _m_next = copy._m_next;

  return *this;
}

std::string
where::format (bool brief) const
{
  assert (_m_formatter != NULL);
  return _m_formatter->format (*this, brief);
}

void
where::reset_1 (uint64_t addr)
{
  _m_addr1 = addr;
  _m_addr2 = _m_addr3 = (uint64_t)-1;
}

void
where::reset_2 (uint64_t addr)
{
  _m_addr2 = addr;
  _m_addr3 = (uint64_t)-1;
}

void
where::reset_3 (uint64_t addr)
{
  _m_addr3 = addr;
}
