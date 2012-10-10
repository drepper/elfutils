/* Convert function descriptor SYM to the function PC value in-place.
   Copyright (C) 2012 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libeblP.h>
#include <assert.h>

const char *
ebl_get_func_pc (Ebl *ebl, struct Dwfl_Module *mod, GElf_Sym *sym)
{
  if (ebl == NULL)
    return NULL;
  assert (sym != NULL);
  assert (GELF_ST_TYPE (sym->st_info) == STT_FUNC);
  if (ebl->get_func_pc == NULL)
    return NULL;
  return ebl->get_func_pc (ebl, mod, sym);
}
