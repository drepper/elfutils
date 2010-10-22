/*
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

#include "../wrap.hh"
#include <cassert>

std::string
wrap (char const *str, size_t width)
{
  return wrap_str (str, width).join ();
}

std::string
sspaces (size_t i)
{
  return spaces (i);
}

int main (void)
{
  assert (sspaces (0) == "");
  assert (sspaces (1) == " ");
  assert (sspaces (2) == "  ");
  assert (sspaces (10) == "          ");
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
