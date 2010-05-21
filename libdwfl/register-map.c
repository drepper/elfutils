/* Handle register maps.
   Copyright (C) 2007-2010 Red Hat, Inc.
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

#include <config.h>
#include "libdwflP.h"


Dwfl_Register_Map *
dwfl_register_map_begin (void)
{
  Dwfl_Register_Map *map = calloc (1, sizeof *map);

  if (map == NULL)
    __libdwfl_seterrno (DWFL_E_NOMEM);

  return map;
}


void
dwfl_register_map_end (map)
     Dwfl_Register_Map *map;
{
  if (map != NULL)
    {
      free (map->types);
      free (map->regs);
      free (map);
    }
}

static int
expand_map (Dwfl_Register_Map *map, int first, int limit)
{
  if (map->regs == NULL)
    {
      map->regs = malloc (sizeof map->regs[0]);
      if (unlikely (map->regs == NULL))
	return -1;
      map->first = first;
      map->limit = limit;
    }
  else if (first < map->first)
    {
      struct map_register *regs
	= realloc (map->regs, (map->limit - first) * sizeof regs[0]);
      if (unlikely (regs == NULL))
	return -1;
      map->regs = memset (regs, 0, (map->first - first) * sizeof regs[0]);
      map->first = first;
    }
  else if (limit > map->limit)
    {
      struct map_register *regs
	= realloc (map->regs, (limit - map->first) * sizeof regs[0]);
      if (unlikely (regs == NULL))
	return -1;
      memset (&regs[map->limit - map->first], 0,
	      (limit - map->limit) * sizeof regs[0]);
      map->limit = limit;
      map->regs = regs;
    }
  return 0;
}

int
dwfl_register_map_populate (map, ref, setno, nhdr, n_name)
     Dwfl_Register_Map *map;
     Dwfl *ref;
     int setno;
     const GElf_Nhdr *nhdr;
     const char *n_name;
{
  size_t offset = 0; // XXX &pr_reg for non-core caller? get from backend?

  if (map == NULL || ref == NULL)
    return -1;

  Dwfl_Module *mod = ref->modulelist; /* XXX */
  GElf_Addr base;
  while (dwfl_module_getelf (mod, &base) == NULL)
    mod = mod->next;
  Dwfl_Error error = __libdwfl_module_getebl (mod); /* XXX */
  Ebl *ebl = mod->ebl;
  if (error != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (error);
      return -1;
    }

  size_t nregloc;
  size_t nitem;
  const Ebl_Register_Location *reglocs;
  const Ebl_Core_Item *items;
  GElf_Word regs_offset;
  int result = ebl_core_note (ebl, nhdr, n_name,
			      &regs_offset, &nregloc, &reglocs, &nitem, &items);
  if (result < 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBEBL);
      return -1;
    }

  inline void install_reg (struct map_register *reg,
			   const Ebl_Register_Location *loc, uint_fast16_t j)
    {
      reg->setno = setno + 1;
      reg->offset = regs_offset + loc->offset - offset;
      if (loc->bits % 8 == 0)
	reg->offset += (loc->bits / 8 + loc->pad) * j;
      else
	abort ();	/* XXX ia64 pr 1-bit */
    }

  if (result > 0)
    {
      result = 0;

      int overlap_setno = -1;
      size_t noverlap = 0;
      size_t total_regs = 0;
      for (size_t i = 0; i < nregloc; ++i)
	{
	  const Ebl_Register_Location *loc = &reglocs[i];

	  int first = loc->regno;
	  int limit = first + loc->count;
	  assert (first < limit);
	  result = expand_map (map, first, limit);
	  if (result < 0)
	    break;

	  for (uint_fast16_t j = 0; j < loc->count; ++j)
	    {
	      struct map_register *reg
		= &map->regs[loc->regno + j - map->first];

	      if (reg->setno != 0 && (overlap_setno < 0
				      || overlap_setno == reg->setno))
		{
		  ++noverlap;
		  overlap_setno = reg->setno;
		}
	      else
		install_reg (reg, loc, j);
	    }

	  total_regs += loc->count;
	  result = map->limit;
	}

      if (result > 0 && noverlap > 0)
	{
	  /* We overlapped with an existing set.
	     See if either the old or the new set is redundant.  */

	  if (noverlap == total_regs)
	    /* The new set is redundant.  Leave it out.  */
	    result = 0;
	  else
	    /* Install the new set, overriding the old.  */
	    for (size_t i = 0; i < nregloc; ++i)
	      {
		const Ebl_Register_Location *loc = &reglocs[i];
		for (uint_fast16_t j = 0; j < loc->count; ++j)
		  install_reg (&map->regs[loc->regno + j - map->first],
			       loc, j);
	      }
	}

      if (result >= 0 && map->ident_setno == 0)
	/* Look for the moniker item.  */
	for (size_t i = 0; i < nitem; ++i)
	  if (items[i].thread_identifier && offset <= items[i].offset)
	    {
	      map->ident_setno = setno + 1;
	      map->ident_type = items[i].type;
	      map->ident_pos = items[i].offset - offset;
	      if (result == 0)
		result = map->limit ?: 1;
	      break;
	    }
    }

  if (result > 0)
    {
      /* Record the set number and n_type value.  */

      if (map->nsets <= setno)
	{
	  GElf_Word *types = realloc (map->types,
				      (setno + 1) * sizeof types[0]);
	  if (unlikely (types == NULL))
	    {
	      __libdwfl_seterrno (DWFL_E_NOMEM);
	      return -1;
	    }
	  map->nsets = setno + 1;
	  map->types = memset (types, 0xff, setno * sizeof types[0]);
	}

      map->types[setno] = nhdr->n_type;
    }

  return result;
}

int
dwfl_register_map (map, regno, offset)
     Dwfl_Register_Map *map;
     int regno;
     GElf_Word *offset;
{
  if (unlikely (map == NULL || regno < map->first || regno >= map->limit))
    return -1;

  const struct map_register *reg = &map->regs[regno - map->first];

  *offset = reg->offset;
  return reg->setno - 1;	/* Unused slot is 0 => -1.  */
}
