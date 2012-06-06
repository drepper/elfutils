/* Scheduler for low_level checks
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

#ifndef DWARFLINT_LOWLEVEL_CHECKS_HH
#define DWARFLINT_LOWLEVEL_CHECKS_HH

#include "checks.hh"

class lowlevel_checks
  : public check<lowlevel_checks>
{
public:
  static checkdescriptor const *descriptor ();
  lowlevel_checks (checkstack &stack, dwarflint &lint);
};

#endif//DWARFLINT_LOWLEVEL_CHECKS_HH
