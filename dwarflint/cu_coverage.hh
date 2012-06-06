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

#ifndef DWARFLINT_CU_COVERAGE_HH
#define DWARFLINT_CU_COVERAGE_HH

#include "check_debug_info_i.hh"
#include "check_debug_loc_range_i.hh"
#include "coverage.hh"
#include "checks.hh"

/** The pass for finalizing cu_coverage.  */
class cu_coverage
  : public check<cu_coverage>
{
  check_debug_info *_m_info;
  check_debug_ranges *_m_ranges;

public:
  static checkdescriptor const *descriptor ();

  coverage cov;

  cu_coverage (checkstack &stack, dwarflint &lint);
};

#endif//DWARFLINT_CU_COVERAGE_HH
