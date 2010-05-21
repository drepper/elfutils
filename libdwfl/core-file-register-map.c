/* Examine a core file for notes describing register data.
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

#include <gelf.h>


// XXX
static inline bool
ebl_core_note_p (Elf *core __attribute__ ((unused)),
		 const GElf_Nhdr *nhdr, const char *name)
{
  inline bool check (const char *core_name, size_t core_namesz)
  {
    if (nhdr->n_namesz == core_namesz)
      return !memcmp (name, core_name, core_namesz);

    /* Also cater to buggy old Linux kernels.  */
    if (nhdr->n_namesz == core_namesz - 1)
      return !memcmp (name, core_name, core_namesz - 1);

    return false;
  }

#define NAME1	"CORE"
#define NAME2	"LINUX"

  return check (NAME1, sizeof NAME1) || check (NAME2, sizeof NAME2);
}


/* We've found a PT_NOTE segment.  Look at each note.  */
static inline int
handle_note (Dwfl *dwfl, Elf *core, const GElf_Phdr *phdr,
	     Dwfl_Register_Map *map, int *setno,
	     GElf_Off *start, GElf_Off *end)
{
  Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset, phdr->p_filesz,
					 ELF_T_NHDR);
  if (data == NULL)
    return -1;

  *start = phdr->p_offset;
  *end = phdr->p_offset + data->d_size;

  int result = 0;
  size_t offset = 0;
  GElf_Nhdr nhdr;
  size_t name_offset;
  size_t desc_offset;
  while (offset < data->d_size
	 && (offset = gelf_getnote (data, offset,
				    &nhdr, &name_offset, &desc_offset)) > 0)
    {
      result = ebl_core_note_p (core, &nhdr, data->d_buf + name_offset);
      if (result < 0)
	break;

      if (result == 0)
	{
	  *start = phdr->p_offset + offset;
	  continue;
	}

      result = dwfl_register_map_populate (map, dwfl, *setno,
					   &nhdr, data->d_buf + name_offset);
      if (result < 0)
	break;
      if (result > 0)
	++*setno;
    }

  return result;
}

// XXX change this interface: return Elf_Data of notes?
int
dwfl_core_file_register_map (dwfl, map, offset, limit)
     Dwfl *dwfl;
     Dwfl_Register_Map **map;
     GElf_Off *offset;
     GElf_Off *limit;
{
  if (dwfl == NULL)
    return -1;

  Elf *core = dwfl->cb_data;

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (core, &ehdr_mem);
  if (ehdr == NULL)
    {
    elf_error:
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return -1;
    }

  *map = dwfl_register_map_begin ();

  int setno = 0;
  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (core, i, &phdr_mem);
      if (phdr == NULL)
	goto elf_error;
      if (phdr->p_type == PT_NOTE)
	{
	  int result = handle_note (dwfl, core, phdr, *map,
				    &setno, offset, limit);
	  if (result < 0)
	    setno = -1;
	  if (result != 0)
	    break;
	}
    }

  if (setno <= 0)
    {
      dwfl_register_map_end (*map);
      *map = NULL;
    }

  return setno;
}

int
dwfl_core_file_read_note (dwfl, map, offset, limit,
			  nsets, offsets, sizes,
			  ident_setno, ident_pos, ident_type,
			  new_offset, next, out_desc_offset)
     Dwfl *dwfl;
     Dwfl_Register_Map *map;
     GElf_Off offset;
     GElf_Off limit;
     int nsets;
     GElf_Off offsets[nsets];
     GElf_Word sizes[nsets];
     int *ident_setno;
     GElf_Word *ident_pos;
     Elf_Type *ident_type;
     GElf_Off *new_offset;
     GElf_Nhdr *next;
     GElf_Off *out_desc_offset;
{
  if (dwfl == NULL || map == NULL || nsets <= map->ident_setno - 1)
    return -1;

  Elf *core = dwfl->cb_data;

  memset (offsets, 0, nsets * sizeof offsets[0]);
  memset (sizes, 0, nsets * sizeof sizes[0]);
  *ident_setno = -1;
  *ident_pos = 0;
  *ident_type = ELF_T_NUM;

  Elf_Data *data = elf_getdata_rawchunk (core, offset, limit - offset,
					 ELF_T_NHDR);
  if (data == NULL)
    return -1;

  int result = 0;
  size_t pos = 0;
  size_t name_offset;
  size_t desc_offset;
  while (pos < data->d_size
	 && (pos = gelf_getnote (data, pos,
				 next, &name_offset, &desc_offset)) > 0
	 && ebl_core_note_p (core, next, data->d_buf + name_offset))
    {
      int i;

      for (i = 0; i < map->nsets; ++i)
	if (map->types[i] == next->n_type)
	  break;
      if (i == map->nsets)
	break;

      if (i == *ident_setno || (i < nsets && sizes[0] != 0))
	{
	  /* This is a repeat set, must be the next thread.  */
	  result = 1;
	  break;
	}

      if (likely (i < nsets))
	{
	  offsets[i] = offset + desc_offset;
	  sizes[i] = next->n_descsz;
	}

      if (i == map->ident_setno - 1)
	{
	  *ident_setno = i;
	  *ident_pos = map->ident_pos;
	  *ident_type = map->ident_type;
	}
    }

  *out_desc_offset = offset + desc_offset;

  if (offset == 0 && limit != 0)
    return -1;

  *new_offset = offset + pos;
  return result;
}
