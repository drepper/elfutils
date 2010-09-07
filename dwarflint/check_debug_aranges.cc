/* Low-level checking of .debug_aranges.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "low.h"
#include "sections.hh"
#include "check_debug_aranges.hh"
#include "check_debug_info.hh"
#include "check_debug_loc_range.hh"
#include "cu_coverage.hh"

checkdescriptor const *
check_debug_aranges::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_aranges")
     .groups ("@low")
     .prereq<typeof (*_m_sec_aranges)> ()
     .prereq<typeof (*_m_info)> ()
     .prereq<typeof (*_m_cu_coverage)> ()
     .description (
"Checks for low-level structure of .debug_aranges.  In addition it\n"
"checks:\n"
" - that relocations are valid.  In ET_REL files that certain fields\n"
"   are relocated\n"
" - for dangling and duplicate CU references\n"
" - for garbage inside padding\n"
" - for zero-length ranges\n"
" - that the ranges cover all the address range covered by CUs\n"
		   ));
  return &cd;
}

check_debug_aranges::check_debug_aranges (checkstack &stack, dwarflint &lint)
  : _m_sec_aranges (lint.check (stack, _m_sec_aranges))
  , _m_info (lint.toplev_check (stack, _m_info))
  , _m_cu_coverage (lint.toplev_check (stack, _m_cu_coverage))
{
  coverage *cov = _m_cu_coverage != NULL ? &_m_cu_coverage->cov : NULL;

  if (!check_aranges_structural (&_m_sec_aranges->file,
				 &_m_sec_aranges->sect,
				 _m_info != NULL
				   ? &_m_info->cus.front () : NULL,
				 cov))
    throw check_base::failed ();
}
