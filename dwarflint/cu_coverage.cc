/* Pedantic checking of DWARF files
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

#include "cu_coverage.hh"
#include "check_debug_info.hh"
#include "check_debug_loc_range.hh"
#include <cstring>

checkdescriptor const *
cu_coverage::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("cu_coverage")
     .hidden ());
  return &cd;
}

cu_coverage::cu_coverage (checkstack &stack, dwarflint &lint)
  : _m_info (lint.check (stack, _m_info))
  , _m_ranges (lint.check_if (_m_info->need_ranges (), stack, _m_ranges))
  , cov (_m_info->cov ()
	 + (_m_ranges != NULL ? _m_ranges->cov () : coverage ()))
{
}
