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

#include "wrap.hh"
#include <cassert>
#include <cstring>

namespace
{
  size_t
  skip_blank (char const *str, size_t pos)
  {
    while (isblank (str[pos]))
      pos++;
    return pos;
  }
}

wrapline_t::wrapline_t (size_t start, size_t end, size_t indent)
  : _m_start (start)
  , _m_end (end)
  , _m_indent (indent)
{
  assert (end >= start);
}

std::string
wrapline_t::build (char const *image) const
{
  assert (_m_end <= std::strlen (image));
  std::string main (image, _m_start, _m_end - _m_start);
  char const *padding = spaces (_m_indent);
  return std::string (padding) + main;
}

wrap_str::wrap_str (char const *str, unsigned width)
  : _m_image (str)
{
  size_t pos = 0;
  bool newline = true;
  size_t indent = 0;
  size_t str_size = std::strlen (str);
  while (pos < str_size)
    {
      size_t last = pos;

      // This is how long a line we can allocate.
      size_t length = width;

      // For newlines, i.e. right after hard EOL (\n) in input string,
      // we look for indentation and bullets.
      if (newline)
	{
	  pos = skip_blank (str, pos);
	  if (pos < str_size && str[pos] == '-')
	    pos = skip_blank (str, pos + 1);
	  indent = pos - last;
	}
      length -= indent;

      // Take the remainder of the line, but don't cross hard EOLs.
      for (; length > 0 && pos < str_size; --length)
	if (str[pos] == '\n')
	  break;
	else
	  pos++;

      // We may have ended mid-word.  Look back to first white space.
      // Look as far back as the end of previous line.
      size_t space = pos;
      for (; space > last; --space)
	if (space == str_size || isspace (str[space]))
	  break;

      // While skipping back, we might end at the very beginning.  If
      // that's the case, we have a word that doesn't fit user limit.
      // Include it whole anyway.  For newline, account for
      // freshly-introduced indent.
      if (space <= last + (newline ? indent : 0))
	{
	  space = pos;
	  while (space < str_size && !isspace (str[space]))
	    space++;
	}

      // We have a line!
      push_back (wrapline_t (last, space, newline ? 0 : indent));

      // Skip useless white space at the end of the line, up to EOL.
      while (space < str_size && isspace (str[space]) && str[space] != '\n')
	space++;

      if (str[space] == '\n')
	{
	  space++;
	  indent = 0;
	  newline = true;
	}
      else
	newline = false;

      pos = space;
    }
}

std::string
wrap_str::join () const
{
  std::string ret;
  for (const_iterator it = begin (); it != end (); ++it)
    ret += build (it) + "\n";
  return ret;
}

std::string
wrap_str::build (wrap_str::const_iterator it) const
{
  return it->build (_m_image);
}

namespace
{
  template <unsigned Max>
  class spc
  {
    char *_m_buf;
    char *_m_endp;
  public:
    spc ()
      : _m_buf (new char[Max])
      , _m_endp (_m_buf + Max - 1)
    {
      std::memset (_m_buf, ' ', Max - 1);
      _m_buf[Max - 1] = 0;
    }
    ~spc ()
    {
      delete [] _m_buf;
    }
    char const *get (size_t n)
    {
      assert (n < Max);
      return _m_endp - n;
    }
  };
}

char const *
spaces (size_t n)
{
  static spc<128> spaces;
  return spaces.get (n);
}
