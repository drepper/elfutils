/* Pseudo-XMLish printing for elfutils::dwarf* tests.
   Copyright (C) 2009 Red Hat, Inc.
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

#include "c++/dwarf"
#include "c++/dwarf_edit"
#include "c++/dwarf_output"

extern void print_die_main (int &argc, char **&argv, unsigned int &depth);

template<typename file>
extern void
print_file (const char *name, const file &dw, const unsigned int limit);

// Explicit instantiations.
extern template void print_file (const char *, const dwarf &,
				 const unsigned int);
extern template void print_file (const char *, const dwarf_edit &,
				 const unsigned int);
extern template void print_file (const char *, const dwarf_output &,
				 const unsigned int);
