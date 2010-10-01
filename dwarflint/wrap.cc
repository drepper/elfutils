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

#include "wrap.hh"
#include <cassert>
#include <cstring>

namespace
{
  size_t
  skip_blank (std::string const &str, size_t pos)
  {
    while (pos < str.size () && isblank (str[pos]))
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
wrapline_t::build (std::string const &image) const
{
  assert (_m_end <= image.size ());
  std::string main = image.substr (_m_start, _m_end - _m_start);
  char padding[_m_indent + 1];
  std::memset (padding, ' ', _m_indent);
  padding[_m_indent] = 0;
  return std::string (padding) + main;
}

wrap_str::wrap_str (std::string const &str, unsigned width)
  : _m_image (str)
{
  size_t pos = 0;
  bool newline = true;
  size_t indent = 0;
  while (pos < str.size ())
    {
      size_t last = pos;

      // This is how long a line we can allocate.
      size_t length = width;

      // For newlines, i.e. right after hard EOL (\n) in input string,
      // we look for indentation and bullets.
      if (newline)
	{
	  pos = skip_blank (str, pos);
	  if (pos < str.size () && str[pos] == '-')
	    pos = skip_blank (str, pos + 1);
	  indent = pos - last;
	}
      length -= indent;

      // Take the remainder of the line, but don't cross hard EOLs.
      for (; length > 0 && pos < str.size (); --length)
	if (str[pos] == '\n')
	  break;
	else
	  pos++;

      // We may have ended mid-word.  Look back to first white space.
      // Look as far back as the end of previous line.
      size_t space = pos;
      for (; space > last; --space)
	if (space == str.size () || isspace (str[space]))
	  break;

      // While skipping back, we might end at the very beginning.  If
      // that's the case, we have a word that doesn't fit user limit.
      // Include it whole anyway.  For newline, account for
      // freshly-introduced indent.
      if (space <= last + (newline ? indent : 0))
	{
	  space = pos;
	  while (space < str.size () && !isspace (str[space]))
	    space++;
	}

      // We have a line!
      push_back (wrapline_t (last, space, newline ? 0 : indent));

      // Skip useless white space at the end of the line, up to EOL.
      while (space < str.size () && isspace (str[space]) && str[space] != '\n')
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
  class tests
  {
    std::string
    wrap (std::string const &str, size_t width)
    {
      return wrap_str (str, width).join ();
    }

  public:
    tests ()
    {
      assert (wrap ("a b c d", 1) == "a\nb\nc\nd\n");
      assert (wrap ("a bbbbb c d", 1) == "a\nbbbbb\nc\nd\n");
      assert (wrap ("a b", 3) == "a b\n");
      assert (wrap (" a b", 3) == " a\n b\n");
      assert (wrap (" a b", 4) == " a b\n");
      assert (wrap (" a b c d", 4) == " a b\n c d\n");
      assert (wrap ("ab cd ef gh ij", 2) == "ab\ncd\nef\ngh\nij\n");
      assert (wrap ("ab cd ef gh ij", 3) == "ab\ncd\nef\ngh\nij\n");
      assert (wrap ("ab cd ef gh ij", 4) == "ab\ncd\nef\ngh\nij\n");
      assert (wrap ("ab cd ef gh ij", 5) == "ab cd\nef gh\nij\n");
      assert (wrap ("", 5) == "");
      assert (wrap ("", 0) == "");
      assert (wrap ("\n", 5) == "\n");
      assert (wrap ("\n\n", 5) == "\n\n");
      assert (wrap ("\n\n", 0) == "\n\n");
      assert (wrap ("ab\ncd ef gh ij", 5) == "ab\ncd ef\ngh ij\n");
      assert (wrap (" - abcd abbb accc", 3) == " - abcd\n   abbb\n   accc\n");
      assert (wrap (" -abcd abbb accc", 3) == " -abcd\n  abbb\n  accc\n");
      assert (wrap ("  abcd abbb accc", 3) == "  abcd\n  abbb\n  accc\n");
    }
  } _;
}
