/*
   Copyright (C) 2011 Red Hat, Inc.
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
