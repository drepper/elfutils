/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009,2010 Red Hat, Inc.
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

#include <assert.h>
#include <error.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
#include <unistd.h>

#include "../libdw/dwarf.h"
#include "../libebl/libebl.h"
#include "../src/dwarfstrings.h"
#include "low.h"
#include "readctx.h"
#include "tables.h"

bool
address_aligned (uint64_t addr, uint64_t align)
{
  return align < 2 || (addr % align == 0);
}

bool
necessary_alignment (uint64_t start, uint64_t length, uint64_t align)
{
  return address_aligned (start + length, align) && length < align;
}

bool
checked_read_uleb128 (struct read_ctx *ctx, uint64_t *ret,
		      struct where *where, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_uleb128 (ctx, ret);
  if (st < 0)
    wr_error (where, ": can't read %s.\n", what);
  else if (st > 0)
    {
      char buf[19]; // 16 hexa digits, "0x", terminating zero
      sprintf (buf, "%#" PRIx64, *ret);
      wr_format_leb128_message (where, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

bool
checked_read_sleb128 (struct read_ctx *ctx, int64_t *ret,
		      struct where *where, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_sleb128 (ctx, ret);
  if (st < 0)
    wr_error (where, ": can't read %s.\n", what);
  else if (st > 0)
    {
      char buf[20]; // sign, "0x", 16 hexa digits, terminating zero
      int64_t val = *ret;
      sprintf (buf, "%s%#" PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
      wr_format_leb128_message (where, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

int
check_sibling_form (dwarf_version_h ver, uint64_t form)
{
  if (!dwver_form_allowed (ver, DW_AT_sibling, form))
    return -2;
  else if (form == DW_FORM_ref_addr)
    return -1;
  else
    return 0;
}

bool
is_location_attrib (uint64_t name)
{
  switch (name)
    {
    case DW_AT_location:
    case DW_AT_frame_base:
    case DW_AT_data_location:
    case DW_AT_data_member_location:
      return true;
    default:
      return false;
    }
}

bool
supported_version (unsigned version,
		   size_t num_supported, struct where *where, ...)
{
  bool retval = false;
  va_list ap;
  va_start (ap, where);
  for (size_t i = 0; i < num_supported; ++i)
    {
      unsigned v = va_arg (ap, unsigned);
      if (version == v)
	{
	  retval = true;
	  break;
	}
    }
  va_end (ap);

  if (!retval)
    wr_error (where, ": unsupported version %d.\n", version);

  return retval;
}

void
check_range_relocations (enum message_category cat,
			 struct where *where,
			 struct elf_file const *file,
			 GElf_Sym *begin_symbol,
			 GElf_Sym *end_symbol,
			 const char *description)
{
  if (begin_symbol != NULL
      && end_symbol != NULL
      && begin_symbol->st_shndx != end_symbol->st_shndx)
    wr_message (cat | mc_impact_2 | mc_reloc, where,
		": %s relocated against different sections (%s and %s).\n",
		description,
		file->sec[begin_symbol->st_shndx].name,
		file->sec[end_symbol->st_shndx].name);
}

static GElf_Rela *
get_rel_or_rela (Elf_Data *data, int ndx,
		 GElf_Rela *dst, size_t type)
{
  if (type == SHT_RELA)
    return gelf_getrela (data, ndx, dst);
  else
    {
      assert (type == SHT_REL);
      GElf_Rel rel_mem;
      if (gelf_getrel (data, ndx, &rel_mem) == NULL)
	return NULL;
      dst->r_offset = rel_mem.r_offset;
      dst->r_info = rel_mem.r_info;
      dst->r_addend = 0;
      return dst;
    }
}

bool
read_rel (struct elf_file *file,
	  struct sec *sec,
	  Elf_Data *reldata,
	  bool elf_64)
{
  assert (sec->rel.type == SHT_REL
	  || sec->rel.type == SHT_RELA);
  bool is_rela = sec->rel.type == SHT_RELA;

  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  size_t entrysize
    = elf_64
    ? (is_rela ? sizeof (Elf64_Rela) : sizeof (Elf64_Rel))
    : (is_rela ? sizeof (Elf32_Rela) : sizeof (Elf32_Rel));
  size_t count = reldata->d_size / entrysize;

  struct where parent = WHERE (sec->id, NULL);
  struct where where = WHERE (is_rela ? sec_rela : sec_rel, NULL);
  where.ref = &parent;

  for (unsigned i = 0; i < count; ++i)
    {
      where_reset_1 (&where, i);

      REALLOC (&sec->rel, rel);
      struct relocation *cur = sec->rel.rel + sec->rel.size++;
      WIPE (*cur);

      GElf_Rela rela_mem, *rela
	= get_rel_or_rela (reldata, i, &rela_mem, sec->rel.type);
      if (rela == NULL)
	{
	  wr_error (&where, ": couldn't read relocation.\n");
	skip:
	  cur->invalid = true;
	  continue;
	}

      int cur_type = GELF_R_TYPE (rela->r_info);
      if (cur_type == 0) /* No relocation.  */
	{
	  wr_message (mc_impact_3 | mc_reloc | mc_acc_bloat, &where,
		      ": NONE relocation is superfluous.\n");
	  goto skip;
	}

      cur->offset = rela->r_offset;
      cur->symndx = GELF_R_SYM (rela->r_info);
      cur->type = cur_type;

      where_reset_2 (&where, cur->offset);

      Elf_Type type = ebl_reloc_simple_type (file->ebl, cur->type);
      int width;

      switch (type)
	{
	case ELF_T_WORD:
	case ELF_T_SWORD:
	  width = 4;
	  break;

	case ELF_T_XWORD:
	case ELF_T_SXWORD:
	  width = 8;
	  break;

	case ELF_T_BYTE:
	case ELF_T_HALF:
	  /* Technically legal, but never used.  Better have dwarflint
	     flag them as erroneous, because it's more likely these
	     are a result of a bug than actually being used.  */
	  {
	    char buf[64];
	    wr_error (&where, ": 8 or 16-bit relocation type %s.\n",
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }

	default:
	  {
	    char buf[64];
	    wr_error (&where, ": invalid relocation %d (%s).\n",
		      cur->type,
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }
	};

      if (cur->offset + width >= sec->data->d_size)
	{
	  wr_error (&where,
		    ": relocation doesn't fall into relocated section.\n");
	  goto skip;
	}

      uint64_t value;
      if (width == 4)
	value = dwarflint_read_4ubyte_unaligned
	  (sec->data->d_buf + cur->offset, file->other_byte_order);
      else
	{
	  assert (width == 8);
	  value = dwarflint_read_8ubyte_unaligned
	    (sec->data->d_buf + cur->offset, file->other_byte_order);
	}

      if (is_rela)
	{
	  if (value != 0)
	    wr_message (mc_impact_2 | mc_reloc, &where,
			": SHR_RELA relocates a place with non-zero value (addend=%#"
			PRIx64", value=%#"PRIx64").\n", rela->r_addend, value);
	  cur->addend = rela->r_addend;
	}
      else
	cur->addend = value;
    }

  /* Sort the reloc section so that the applicable addresses of
     relocation entries are monotonously increasing.  */
  int compare (const void *a, const void *b)
  {
    return ((struct relocation *)a)->offset
      - ((struct relocation *)b)->offset;
  }

  qsort (sec->rel.rel, sec->rel.size,
	 sizeof (*sec->rel.rel), &compare);
  return true;
}
