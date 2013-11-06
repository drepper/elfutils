/* Provide virtual symbols from backend.
   Copyright (C) 2013 Red Hat, Inc.
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

bool
ebl_init_symbols (Ebl *ebl, size_t syments, GElf_Addr main_bias,
		  ebl_getsym_t *getsym, void *arg, size_t *ebl_symentsp,
		  int *ebl_first_globalp)
{
  *ebl_symentsp = 0;
  *ebl_first_globalp = 0;
  if (ebl == NULL)
    return false;
  if (ebl->init_symbols == NULL)
    return true;
  return ebl->init_symbols (ebl, syments, main_bias, getsym, arg, ebl_symentsp,
			    ebl_first_globalp);
}

const char *
ebl_get_symbol (Ebl *ebl, size_t ndx, GElf_Sym *symp, GElf_Word *shndxp)
{
  if (ebl == NULL || ebl->get_symbol == NULL)
    return NULL;
  return ebl->get_symbol (ebl, ndx, symp, shndxp);
}
