/* Initialization of high-level check context
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.

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

#include "highlevel_check.hh"
#include "lowlevel_checks.hh"
#include "files.hh"

open_highlevel_dwarf::open_highlevel_dwarf (checkstack &stack, dwarflint &lint)
  : _m_dwfl ((lint.check<lowlevel_checks> (stack),
	      files::open_dwfl ()))
  , c_dw (files::open_dwarf (_m_dwfl, lint.fname (), lint.fd ()))
  , dw (files::open_dwarf (c_dw))
{}

open_highlevel_dwarf::~open_highlevel_dwarf ()
{
  dwfl_end (_m_dwfl);
}
