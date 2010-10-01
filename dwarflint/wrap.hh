/* Pedantic checking of DWARF files

   Copyright (C) 2010 Red Hat, Inc.
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
  std::string build (std::string const &image) const;
};

class wrap_str
  : private std::vector<wrapline_t>
{
  std::string const &_m_image;

public:
  typedef std::vector<wrapline_t> super_t;
  using super_t::operator [];
  using super_t::size;
  using super_t::const_iterator;
  using super_t::begin;
  using super_t::end;
  using super_t::empty;

  wrap_str (std::string const &str, unsigned width);

  std::string join () const;
  std::string build (const_iterator it) const;
};

#endif//DWARFLINT_WRAP_HH
