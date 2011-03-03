/*
   Copyright (C) 2011 Red Hat, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>

#include "config.h"
#include "../all-dies-it.hh"
#include "../../libdw/c++/dwarf"

using namespace elfutils;

int
main (int argc, char ** argv)
{
  assert (argc == 2);
  char const *fn = argv[1];
  assert (fn != NULL);

  int fd = open (fn, O_RDONLY);
  assert (fd >= 0);

  Dwarf *cdw = dwarf_begin (fd, DWARF_C_READ);
  assert (cdw != NULL);

  dwarf dw = cdw;
  for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
       it != all_dies_iterator<dwarf> (); ++it)
    std::cout << std::hex << "0x" << (*it).offset () << std::endl;
}
