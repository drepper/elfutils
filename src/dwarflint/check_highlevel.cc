/* Initialization of high-level check context
   Copyright (C) 2009 Red Hat, Inc.
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

  // xxx this will throw an exception on <c++/dwarf> or <libdw.h>
  // failure.  We need to catch it and convert to check_base::failed.

#include "checks-high.hh" // xxx rename
#include "../libdwfl/libdwfl.h"

Dwarf *
dwarf_handle_loader::get_dwarf_handle (int fd)
{
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  /*
    static const Dwfl_Callbacks callbacks =
    {
    .section_address = dwfl_offline_section_address,
    .find_debuginfo = find_no_debuginfo
    };
    Dwfl *dwfl = dwfl_begin (&callbacks);
    if (unlikely (dwfl == NULL))
      throw check_base::failed ();
    */
  return dwarf_begin_elf (elf, DWARF_C_READ, NULL);
}
