/* Dwarf version tables, C binding.

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

#ifndef DWARFLINT_TABLES_H
#define DWARFLINT_TABLES_H

#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  struct dwarf_version;
  typedef struct dwarf_version const *dwarf_version_h;

  dwarf_version_h get_dwarf_version (unsigned version)
    __attribute__ ((pure));

  bool dwver_form_valid (dwarf_version_h ver, int form);

  bool dwver_form_allowed (dwarf_version_h ver, int attr, int form);

  bool dwver_form_allowed_in (dwarf_version_h ver, int attr,int form, int tag);

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_TABLES_H
