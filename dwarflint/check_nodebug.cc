/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

#include "checks.hh"
#include "options.h"
#include "messages.h"
#include "sections.hh"

class check_nodebug
  : public check<check_nodebug>
{
public:
  static checkdescriptor const *descriptor ()
  {
    static checkdescriptor cd
      (checkdescriptor::create ("check_nodebug")
       .groups ("@low")
       .description (
"Checks that there are at least essential debuginfo sections present\n"
"in the ELF file."));
    return &cd;
  }

  check_nodebug (checkstack &stack, dwarflint &lint);

private:
  void not_available (section_id sec_id)
  {
    wr_error (WHERE (sec_id, NULL))
      << "data not found." << std::endl;
  }

  template <section_id sec_id>
  void request (checkstack &stack, dwarflint &lint)
  {
    if (lint.toplev_check<section<sec_id> > (stack) == NULL)
      not_available (sec_id);
  }

};

static reg<check_nodebug> reg_nodebug;

check_nodebug::check_nodebug (checkstack &stack, dwarflint &lint)
{
  if (tolerate_nodebug)
    return;

  // We demand .debug_info and .debug_abbrev, the rest is optional.
  // Presence of the other sections is (or should be) requested if
  // there are pending references from .debug_info.
  request<sec_abbrev> (stack, lint);
  request<sec_info> (stack, lint);
}
