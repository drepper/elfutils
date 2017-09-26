/* Pedantic checking of DWARF files.
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

#ifndef _DWARFLINT_FILES_H_
#define _DWARFLINT_FILES_H_

#include "../libdwfl/libdwfl.h"
#include "../libdw/c++/dwarf"

// The functions in this module do their own error handling, and throw
// std::runtime_error with descriptive error message on error.
namespace files
{
  int open (char const *fname);

  Dwfl *open_dwfl ()
    __attribute__ ((nonnull, malloc));

  Dwarf *open_dwarf (Dwfl *dwfl, char const *fname, int fd)
    __attribute__ ((nonnull, malloc));

  elfutils::dwarf open_dwarf (Dwarf *dw);
}

#endif /* _DWARFLINT_FILES_H_ */
