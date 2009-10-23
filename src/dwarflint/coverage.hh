/* Coverage analysis, C++ support.

   Copyright (C) 2008,2009 Red Hat, Inc.
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

#ifndef DWARFLINT_COVERAGE_HH
#define DWARFLINT_COVERAGE_HH

#include <string>
#include <sstream>
#include "coverage.h"

std::string range_fmt (uint64_t start, uint64_t end);

namespace cov
{
  class _format_base
  {
  protected:
    std::string const &_m_delim;
    std::ostringstream _m_os;
    bool _m_seen;

    inline bool fmt (uint64_t start, uint64_t length)
    {
      if (_m_seen)
	_m_os << _m_delim;
      _m_os << range_fmt (start, start + length);
      _m_seen = true;
      return true;
    }

    static bool
    wrap_fmt (uint64_t start, uint64_t length, void *data)
    {
      _format_base *self = static_cast <_format_base *> (data);
      return self->fmt (start, length);
    }

    _format_base (std::string const &delim)
      : _m_delim (delim),
	_m_seen (false)
    {
      _m_os << std::hex;
    }

  public:
    operator std::string () const
    {
      return _m_os.str ();
    }
  };

  struct format_ranges
    : public _format_base
  {
    format_ranges (coverage const &cov, std::string const &delim = ", ")
      : _format_base (delim)
    {
      coverage_find_ranges (&cov, &wrap_fmt, this);
    }
  };

  struct format_holes
    : public _format_base
  {
    format_holes (coverage const &cov, uint64_t start, uint64_t length,
		  std::string const &delim = ", ")
      : _format_base (delim)
    {
      coverage_find_holes (&cov, start, length, &wrap_fmt, this);
    }
  };
}

inline std::ostream &
operator << (std::ostream &os, cov::format_ranges const &obj)
{
  return os << std::string (obj);
}

inline std::ostream &
operator << (std::ostream &os, cov::format_holes const &obj)
{
  return os << std::string (obj);
}
#endif//DWARFLINT_COVERAGE_HH
