/* Pedantic checking of DWARF files

   Copyright (C) 2010 Red Hat, Inc.
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

#ifndef DWARFLINT_WRAP_HH
#define DWARFLINT_WRAP_HH

#include <string>
#include <vector>

class wrapline_t
{
  size_t _m_start;
  size_t _m_end;
  size_t _m_indent;

public:
  wrapline_t (size_t start, size_t end, size_t indent);
  std::string build (char const *image) const;
};

class wrap_str
  : private std::vector<wrapline_t>
{
  char const *_m_image;

public:
  typedef std::vector<wrapline_t> super_t;
  using super_t::operator [];
  using super_t::size;
  using super_t::const_iterator;
  using super_t::begin;
  using super_t::end;
  using super_t::empty;

  wrap_str (char const *str, unsigned width);

  std::string join () const;
  std::string build (const_iterator it) const;
};

char const *spaces (size_t n);

#endif//DWARFLINT_WRAP_HH
