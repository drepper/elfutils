/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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

#include "checks.hh"
#include "messages.hh"
#include "sections.hh"
#include "option.hh"

static void_option ignore_missing
  ("Don't complain if files have no DWARF at all",
   "nodebug:ignore", 'i');

class check_nodebug
  : public check<check_nodebug>
{
public:
  static checkdescriptor const *descriptor ()
  {
    static checkdescriptor cd
      (checkdescriptor::create ("check_nodebug")
       .groups ("@low")
       .option (ignore_missing)
       .description (
"Checks that there are at least essential debuginfo sections present "
"in the ELF file.\n"));
    return &cd;
  }

  check_nodebug (checkstack &stack, dwarflint &lint);

private:
  void not_available (section_id sec_id)
  {
    wr_error (section_locus (sec_id))
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
  if (ignore_missing)
    return;

  // We demand .debug_info and .debug_abbrev, the rest is optional.
  // Presence of the other sections is (or should be) requested if
  // there are pending references from .debug_info.
  request<sec_abbrev> (stack, lint);
  request<sec_info> (stack, lint);
}
