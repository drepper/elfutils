/* Scheduler for low_level checks
   Copyright (C) 2010, 2011 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "lowlevel_checks.hh"
#include "sections.hh"
#include "check_debug_info.hh"
#include "check_debug_abbrev.hh"
#include "check_debug_aranges.hh"
#include "check_debug_pub.hh"
#include "check_debug_loc_range.hh"
#include "check_debug_line.hh"

checkdescriptor const *
lowlevel_checks::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("lowlevel_checks")
     .hidden ());
  return &cd;
}

static reg<lowlevel_checks> reg_lowlevel_checks;

namespace
{
  template<class T> struct do_check {
    static bool check (checkstack &stack, dwarflint &lint) {
      return lint.toplev_check<T> (stack);
    }
  };

  // There is no separate check_debug_str pass.  Make a stub so that
  // we can do it all in one macro-expanded sweep below.
#define STUBBED_CHECK(NAME)						\
  struct check_debug_##NAME {};						\
  template<> struct do_check<check_debug_##NAME> {			\
    static bool check (__attribute__ ((unused)) checkstack &stack,	\
		       __attribute__ ((unused)) dwarflint &lint)	\
    {									\
      return true;							\
    }									\
  }
  STUBBED_CHECK(str);
  STUBBED_CHECK(mac);
#undef STUBBED_CHECK
}

lowlevel_checks::lowlevel_checks (checkstack &stack, dwarflint &lint)
{
  // Then check all the debug sections that are there.  For each
  // existing section request that the check passes.  Re-requesting
  // already-passed checks is OK, the scheduler caches it.
  bool passed = true;

#define SEC(NAME)							\
  section<sec_##NAME> *NAME =						\
    lint.toplev_check<section<sec_##NAME> > (stack);			\
  if (NAME != NULL)							\
    if (!do_check<check_debug_##NAME>::check (stack, lint))		\
      passed = false;

  DEBUGINFO_SECTIONS;
#undef SEC

  lint.check<check_debug_info_refs> (stack);

  if (!passed)
    throw check_base::failed ();
}
