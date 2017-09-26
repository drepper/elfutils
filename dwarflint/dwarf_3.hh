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

#ifndef DWARFLINT_DWARF_3_HH
#define DWARFLINT_DWARF_3_HH

#include "dwarf_version_i.hh"

/// Pure DWARF 3 extension.
dwarf_version const *dwarf_3_ext ();

/// DWARF 3 and below.
dwarf_version const *dwarf_3 ();

#endif//DWARFLINT_DWARF_3_HH
