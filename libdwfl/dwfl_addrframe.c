/*
   Copyright (C) 2006, 2009 Red Hat, Inc.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include "libdwflP.h"
#include "../libdw/cfi.h"

/* Return -1 for hard error, 0 for address match, 1 for no match.  */
static int
try_cfi (Dwarf_CFI *cfi, Dwarf_Addr *bias, bool hard,
	 Dwarf_Addr address, Dwarf_Frame **frame)
{
  int result = INTUSE(dwarf_cfi_addrframe) (cfi, address - *bias, frame);
  if (result != 0)
    {
      if (hard)
	__libdwfl_seterrno (DWFL_E_LIBDW);
      else
	{
	  int err = INTUSE(dwarf_errno) ();
	  if (err == DWARF_E_NO_MATCH)
	    return 1;
	  __libdwfl_seterrno (DWFL_E (LIBDW, err));
	}
    }
  return result;
}

int
dwfl_addrframe (dwfl, address, frame, bias)
     Dwfl *dwfl;
     Dwarf_Addr address;
     Dwarf_Frame **frame;
     Dwarf_Addr *bias;
{
  Dwfl_Module *mod = INTUSE(dwfl_addrmodule) (dwfl, address);
  if (mod == NULL)
    return -1;

  /* Try to get a .debug_frame match first, then a .eh_frame match.  */
  int result = try_cfi (INTUSE(dwfl_module_dwarf_cfi) (mod, bias), bias,
			false, address, frame);
  if (result > 0)
    result = try_cfi (INTUSE(dwfl_module_eh_cfi) (mod, bias), bias,
		      true, address, frame);

  return result;
}
