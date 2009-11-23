/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009 Red Hat, Inc.
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
#include "dwarfstrings.h"
#include "low.h"
#include "readctx.h"
#include "config.h"
#include "dwarf-opcodes.h"
#include "tables.h"

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64


static struct cu *cu_find_cu (struct cu *cu_chain, uint64_t offset);

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

/* The value passed back in uint64_t VALUEP may actually be
   type-casted signed quantity.  WHAT and WHERE describe error message
   and context for LEB128 loading.

   If IS_BLOCKP is non-NULL, block values are accepted, and *IS_BLOCKP
   is initialized depending on whether FORM is a block form.  For
   block forms, the value passed back in VALUEP is block length.  */
static bool
read_ctx_read_form (struct read_ctx *ctx, int address_size, uint8_t form,
		    uint64_t *valuep, struct where *where, const char *what,
		    bool *is_blockp)
{
  if (is_blockp != NULL)
    *is_blockp = false;
  switch (form)
    {
    case DW_FORM_addr:
      return read_ctx_read_offset (ctx, address_size == 8, valuep);
    case DW_FORM_udata:
      return checked_read_uleb128 (ctx, valuep, where, what);
    case DW_FORM_sdata:
      return checked_read_sleb128 (ctx, (int64_t *)valuep, where, what);
    case DW_FORM_data1:
      {
	uint8_t v;
	if (!read_ctx_read_ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data2:
      {
	uint16_t v;
	if (!read_ctx_read_2ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data4:
      {
	uint32_t v;
	if (!read_ctx_read_4ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data8:
      return read_ctx_read_8ubyte (ctx, valuep);
    };

  if (is_blockp != NULL)
    {
      int dform;
      switch (form)
	{
#define HANDLE(BFORM, DFORM)			\
	  case BFORM:				\
	    dform = DFORM;			\
	    break
	  HANDLE (DW_FORM_block, DW_FORM_udata);
	  HANDLE (DW_FORM_block1, DW_FORM_data1);
	  HANDLE (DW_FORM_block2, DW_FORM_data2);
	  HANDLE (DW_FORM_block4, DW_FORM_data4);
#undef HANDLE
	default:
	  return false;
	}

      *is_blockp = true;
      return read_ctx_read_form (ctx, address_size, dform,
				 valuep, where, what, NULL)
	&& read_ctx_skip (ctx, *valuep);
    }

  return false;
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

void
cu_free (struct cu *cu_chain)
{
  for (struct cu *it = cu_chain; it != NULL; )
    {
      addr_record_free (&it->die_addrs);

      struct cu *temp = it;
      it = it->next;
      free (temp);
    }
}

static struct cu *
cu_find_cu (struct cu *cu_chain, uint64_t offset)
{
  for (struct cu *it = cu_chain; it != NULL; it = it->next)
    if (it->head->offset == offset)
      return it;
  return NULL;
}

bool
read_size_extra (struct read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		 int *offset_sizep, struct where *wh)
{
  if (size32 == DWARF3_LENGTH_64_BIT)
    {
      if (!read_ctx_read_8ubyte (ctx, sizep))
	{
	  wr_error (wh, ": can't read 64bit CU length.\n");
	  return false;
	}

      *offset_sizep = 8;
    }
  else if (size32 >= DWARF3_LENGTH_MIN_ESCAPE_CODE)
    {
      wr_error (wh, ": unrecognized CU length escape value: "
		"%" PRIx32 ".\n", size32);
      return false;
    }
  else
    {
      *sizep = size32;
      *offset_sizep = 4;
    }

  return true;
}

bool
check_zero_padding (struct read_ctx *ctx,
		    enum message_category category,
		    struct where const *wh)
{
  assert (ctx->ptr != ctx->end);
  const unsigned char *save_ptr = ctx->ptr;
  while (!read_ctx_eof (ctx))
    if (*ctx->ptr++ != 0)
      {
	ctx->ptr = save_ptr;
	return false;
      }

  wr_message_padding_0 (category, wh,
			(uint64_t)(save_ptr - ctx->begin),
			(uint64_t)(ctx->end - ctx->begin));
  return true;
}

static enum section_id
reloc_target_loc (uint8_t opcode)
{
  switch (opcode)
    {
    case DW_OP_call2:
    case DW_OP_call4:
      return sec_info;

    case DW_OP_addr:
      return rel_address;

    case DW_OP_call_ref:
      assert (!"Can't handle call_ref!");
    };

  printf ("XXX don't know how to handle opcode=%s\n",
	  dwarf_locexpr_opcode_string (opcode));

  return rel_value;
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
			 struct elf_file *file,
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

bool
read_address_size (bool elf_64,
		   struct read_ctx *ctx,
		   int *address_sizep,
		   struct where const *where)
{
  uint8_t address_size;
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      wr_error (where, ": can't read address size.\n");
      return false;
    }

  if (address_size != 4 && address_size != 8)
    {
      /* Keep going.  Deduce the address size from ELF header, and try
	 to parse it anyway.  */
      wr_error (where,
		": invalid address size: %d (only 4 or 8 allowed).\n",
		address_size);
      address_size = elf_64 ? 8 : 4;
    }
  else if ((address_size == 8) != elf_64)
    /* Keep going, we may still be able to parse it.  */
    wr_error (where,
	      ": CU reports address size of %d in %d-bit ELF.\n",
	      address_size, elf_64 ? 64 : 32);

  *address_sizep = address_size;
  return true;
}

static void
compare_coverage (struct elf_file *file,
		  struct coverage *coverage, struct coverage *other,
		  enum section_id id, char const *what)
{
  struct coverage *cov = coverage_clone (coverage);
  coverage_remove_all (cov, other);

  bool hole (uint64_t start, uint64_t length, void *user)
  {
    /* We need to check alignment vs. the covered section.  Find
       where the hole lies.  */
    struct elf_file *elf = user;
    struct sec *sec = NULL;
    for (size_t i = 1; i < elf->size; ++i)
      {
	struct sec *it = elf->sec + i;
	GElf_Shdr *shdr = &it->shdr;
	Elf64_Addr s_end = shdr->sh_addr + shdr->sh_size;
	if (start >= shdr->sh_addr && start + length < s_end)
	  {
	    sec = it;
	    /* Simply assume the first section that the hole
	       intersects. */
	    break;
	  }
      }

    if (sec == NULL
	|| !necessary_alignment (start, length, sec->shdr.sh_addralign))
      {
	char buf[128];
	wr_message (mc_aranges | mc_impact_3, &WHERE (id, NULL),
		    ": addresses %s are covered with CUs, but not with %s.\n",
		    range_fmt (buf, sizeof buf, start, start + length), what);
      }

    if (sec == NULL)
      wr_error (NULL, "Couldn't find the section containing the above hole.\n");

    return true;
  }

  coverage_find_ranges (cov, &hole, file);
  coverage_free (cov);
}

/* COVERAGE is portion of address space covered by CUs (either via
   low_pc/high_pc pairs, or via DW_AT_ranges references).  If
   non-NULL, analysis of arange coverage is done against that set. */
bool
check_aranges_structural (struct elf_file *file,
			  struct sec *sec,
			  struct cu *cu_chain,
			  struct coverage *coverage)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  bool retval = true;

  struct coverage *aranges_coverage
    = coverage != NULL ? calloc (1, sizeof (struct coverage)) : NULL;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec_aranges, NULL);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));
      const unsigned char *atab_begin = ctx.ptr;

      inline void aranges_coverage_add (uint64_t begin, uint64_t length)
      {
	if (coverage_is_overlap (aranges_coverage, begin, length)
	    && !be_gnu && !be_tolerant)
	  {
	    char buf[128];
	    /* Not a show stopper, this shouldn't derail high-level.  */
	    wr_message (mc_aranges | mc_impact_2 | mc_error, &where,
			": the range %s overlaps with another one.\n",
			range_fmt (buf, sizeof buf, begin, begin + length));
	  }

    	coverage_add (aranges_coverage, begin, length);
      }

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      int offset_size;
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (&where, ": can't read table length.\n");
	  return false;
	}
      if (!read_size_extra (&ctx, size32, &size, &offset_size, &where))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *atab_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, atab_begin, atab_end))
	{
	not_enough:
	  wr_error (&where, PRI_NOT_ENOUGH, "next table");
	  return false;
	}

      sub_ctx.ptr = ctx.ptr;

      /* Version.  */
      uint16_t version;
      if (!read_ctx_read_2ubyte (&sub_ctx, &version))
	{
	  wr_error (&where, ": can't read version.\n");
	  retval = false;
	  goto next;
	}
      if (!supported_version (version, 1, &where, 2))
	{
	  retval = false;
	  goto next;
	}

      /* CU offset.  */
      uint64_t cu_offset;
      uint64_t ctx_offset = sub_ctx.ptr - ctx.begin;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &cu_offset))
	{
	  wr_error (&where, ": can't read debug info offset.\n");
	  retval = false;
	  goto next;
	}

      struct relocation *rel;
      if ((rel = relocation_next (&sec->rel, ctx_offset,
				  &where, skip_mismatched)))
	relocate_one (file, &sec->rel, rel, offset_size,
		      &cu_offset, &where, sec_info, NULL);
      else if (file->ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_aranges | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (cu_chain != NULL && (cu = cu_find_cu (cu_chain, cu_offset)) == NULL)
	wr_error (&where, ": unresolved reference to " PRI_CU ".\n", cu_offset);

      struct where where_cudie;
      if (cu != NULL)
	{
	  where_cudie = WHERE (sec_info, NULL);
	  where_reset_1 (&where_cudie, cu->cudie_offset);
	  where.ref = &where_cudie;
	  where_cudie.formatting = wf_cudie;
	  if (cu->has_arange)
	    wr_message (mc_impact_2 | mc_aranges | mc_header, &where,
			": there has already been arange section for this CU.\n");
	  else
	    cu->has_arange = true;
	}

      /* Address size.  */
      int address_size;
      if (!read_address_size (file, &sub_ctx, &address_size, &where))
	{
	  retval = false;
	  goto next;
	}

      /* Segment size.  */
      uint8_t segment_size;
      if (!read_ctx_read_ubyte (&sub_ctx, &segment_size))
	{
	  wr_error (&where, ": can't read unit segment size.\n");
	  retval = false;
	  goto next;
	}
      if (segment_size != 0)
	{
	  wr_warning (&where, ": dwarflint can't handle segment_size != 0.\n");
	  retval = false;
	  goto next;
	}


      /* 7.20: The first tuple following the header in each set begins
	 at an offset that is a multiple of the size of a single tuple
	 (that is, twice the size of an address). The header is
	 padded, if necessary, to the appropriate boundary.  */
      const uint8_t tuple_size = 2 * address_size;
      uint64_t off = read_ctx_get_offset (&sub_ctx);
      if ((off % tuple_size) != 0)
	{
	  uint64_t noff = ((off / tuple_size) + 1) * tuple_size;
	  for (uint64_t i = off; i < noff; ++i)
	    {
	      uint8_t c;
	      if (!read_ctx_read_ubyte (&sub_ctx, &c))
		{
		  wr_error (&where,
			    ": section ends after the header, "
			    "but before the first entry.\n");
		  retval = false;
		  goto next;
		}
	      if (c != 0)
		wr_message (mc_impact_2 | mc_aranges | mc_header, &where,
			    ": non-zero byte at 0x%" PRIx64
			    " in padding before the first entry.\n",
			    read_ctx_get_offset (&sub_ctx));
	    }
	}
      assert ((read_ctx_get_offset (&sub_ctx) % tuple_size) == 0);

      while (!read_ctx_eof (&sub_ctx))
	{
	  /* We would like to report aranges the same way that readelf
    	     does.  But readelf uses index of the arange in the array
    	     as returned by dwarf_getaranges, which sorts the aranges
    	     beforehand.  We don't want to disturb the memory this
    	     way, the better to catch structural errors accurately.
    	     So report arange offset instead.  If this becomes a
    	     problem, we will achieve this by two-pass analysis.  */
	  where_reset_2 (&where, read_ctx_get_offset (&sub_ctx));

	  /* Record address.  */
	  uint64_t address;
	  ctx_offset = sub_ctx.ptr - ctx.begin;
	  bool address_relocated = false;
	  if (!read_ctx_read_var (&sub_ctx, address_size, &address))
	    {
	      wr_error (&where, ": can't read address field.\n");
	      retval = false;
	      goto next;
	    }

    	  if ((rel = relocation_next (&sec->rel, ctx_offset,
				      &where, skip_mismatched)))
	    {
	      address_relocated = true;
	      relocate_one (file, &sec->rel, rel, address_size,
			    &address, &where, rel_address, NULL);
	    }
	  else if (file->ehdr.e_type == ET_REL && address != 0)
	    wr_message (mc_impact_2 | mc_aranges | mc_reloc, &where,
			PRI_LACK_RELOCATION, "address field");

	  /* Record length.  */
	  uint64_t length;
	  if (!read_ctx_read_var (&sub_ctx, address_size, &length))
	    {
	      wr_error (&where, ": can't read length field.\n");
	      retval = false;
	      goto next;
	    }

	  if (address == 0 && length == 0 && !address_relocated)
	    break;

	  if (length == 0)
	    /* DWARF 3 spec, 6.1.2 Lookup by Address: Each descriptor
	       is a pair consisting of the beginning address [...],
	       followed by the _non-zero_ length of that range.  */
	    wr_error (&where, ": zero-length address range.\n");
	  /* Skip coverage analysis if we have errors.  */
	  else if (retval && aranges_coverage)
	    aranges_coverage_add (address, length);
	}

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_aranges,
				  &WHERE (where.section, NULL)))
	{
	  wr_message_padding_n0 (mc_aranges | mc_error,
				 &WHERE (where.section, NULL),
				 read_ctx_get_offset (&sub_ctx),
				 read_ctx_get_offset (&sub_ctx) + size);
	  retval = false;
	}

    next:
      if (!read_ctx_skip (&ctx, size))
	/* A "can't happen" error.  */
	goto not_enough;
    }

  if (aranges_coverage != NULL)
    {
      compare_coverage (file, coverage, aranges_coverage,
			sec_aranges, "aranges");
      coverage_free (aranges_coverage);
    }

  return retval;
}

bool
check_pub_structural (struct elf_file *file,
		      struct sec *sec,
		      struct cu *cu_chain)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);
  bool retval = true;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec->id, NULL);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));
      const unsigned char *set_begin = ctx.ptr;

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      int offset_size;
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (&where, ": can't read table length.\n");
	  return false;
	}
      if (!read_size_extra (&ctx, size32, &size, &offset_size, &where))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, set_begin, set_end))
	{
	not_enough:
	  wr_error (&where, PRI_NOT_ENOUGH, "next set");
	  return false;
	}
      sub_ctx.ptr = ctx.ptr;

      /* Version.  */
      uint16_t version;
      if (!read_ctx_read_2ubyte (&sub_ctx, &version))
	{
	  wr_error (&where, ": can't read set version.\n");
	  retval = false;
	  goto next;
	}
      if (!supported_version (version, 1, &where, 2))
	{
	  retval = false;
	  goto next;
	}

      /* CU offset.  */
      uint64_t cu_offset;  /* Offset of related CU.  */
      uint64_t ctx_offset = sub_ctx.ptr - ctx.begin;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &cu_offset))
	{
	  wr_error (&where, ": can't read debug info offset.\n");
	  retval = false;
	  goto next;
	}

      struct relocation *rel;
      if ((rel = relocation_next (&sec->rel, ctx_offset,
				  &where, skip_mismatched)))
	relocate_one (file, &sec->rel, rel, offset_size,
		      &cu_offset, &where, sec_info, NULL);
      else if (file->ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_pubtables | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (cu_chain != NULL && (cu = cu_find_cu (cu_chain, cu_offset)) == NULL)
	wr_error (&where, ": unresolved reference to " PRI_CU ".\n", cu_offset);
      if (cu != NULL)
	{
	  where.ref = &cu->head->where;
	  bool *has = sec->id == sec_pubnames
			? &cu->has_pubnames : &cu->has_pubtypes;
	  if (*has)
	    wr_message (mc_impact_2 | mc_pubtables | mc_header, &where,
			": there has already been section for this CU.\n");
	  else
	    *has = true;
	}

      /* Covered length.  */
      uint64_t cu_len;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &cu_len))
	{
	  wr_error (&where, ": can't read covered length.\n");
	  retval = false;
	  goto next;
	}
      if (cu != NULL && cu_len != cu->head->total_size)
	{
	  wr_error (&where,
		    ": the table covers length %" PRId64
		    " but CU has length %" PRId64 ".\n",
		    cu_len, cu->head->total_size);
	  retval = false;
	  goto next;
	}

      /* Records... */
      while (!read_ctx_eof (&sub_ctx))
	{
	  ctx_offset = sub_ctx.ptr - ctx.begin;
	  where_reset_2 (&where, ctx_offset);

	  uint64_t offset;
	  if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &offset))
	    {
	      wr_error (&where, ": can't read offset field.\n");
	      retval = false;
	      goto next;
	    }
	  if (offset == 0)
	    break;

	  if (cu != NULL
	      && !addr_record_has_addr (&cu->die_addrs,
					offset + cu->head->offset))
	    {
	      wr_error (&where,
			": unresolved reference to " PRI_DIE ".\n", offset);
	      retval = false;
	      goto next;
	    }

	  uint8_t c;
	  do
	    if (!read_ctx_read_ubyte (&sub_ctx, &c))
	      {
		wr_error (&where, ": can't read symbol name.\n");
		retval = false;
		goto next;
	      }
	  while (c);
	}

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_pubtables,
				  &WHERE (sec->id, NULL)))
	{
	  wr_message_padding_n0 (mc_pubtables | mc_error,
				 &WHERE (sec->id, NULL),
				 read_ctx_get_offset (&sub_ctx),
				 read_ctx_get_offset (&sub_ctx) + size);
	  retval = false;
	}

    next:
      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (retval)
    relocation_skip_rest (&sec->rel, sec->id);

  return retval;
}


/* Operands are passed back as attribute forms.  In particular,
   DW_FORM_dataX for X-byte operands, DW_FORM_[us]data for
   ULEB128/SLEB128 operands, and DW_FORM_addr for 32b/64b operands.
   If the opcode takes no operands, 0 is passed.

   Return value is false if we couldn't determine (i.e. invalid
   opcode).
 */
static bool
get_location_opcode_operands (uint8_t opcode, uint8_t *op1, uint8_t *op2)
{
  switch (opcode)
    {
#define DW_OP_2(OPCODE, OP1, OP2) \
      case OPCODE: *op1 = OP1; *op2 = OP2; return true;
#define DW_OP_1(OPCODE, OP1) DW_OP_2(OPCODE, OP1, 0)
#define DW_OP_0(OPCODE) DW_OP_2(OPCODE, 0, 0)

      DW_OP_OPERANDS

#undef DEF_DW_OP_2
#undef DEF_DW_OP_1
#undef DEF_DW_OP_0
    default:
      return false;
    };
}

static bool
op_read_form (struct elf_file *file,
	      struct read_ctx *ctx,
	      struct cu *cu,
	      uint64_t init_off,
	      struct relocation_data *reloc,
	      int opcode,
	      int form,
	      uint64_t *valuep,
	      char const *str,
	      struct where *where)
{
  if (form == 0)
    return true;

  bool isblock;
  uint64_t off = read_ctx_get_offset (ctx) + init_off;

  if (!read_ctx_read_form (ctx, cu->head->address_size, form,
			   valuep, where, str, &isblock))
    {
      wr_error (where, ": opcode \"%s\": can't read %s (form \"%s\").\n",
		dwarf_locexpr_opcode_string (opcode),
		str, dwarf_form_string (form));
      return false;
    }

  /* For non-block forms, allow relocation of the datum.  For block
     form, allow relocation of block contents, but not the
     block length).  */

  struct relocation *rel;
  if ((rel = relocation_next (reloc, off,
			      where, skip_mismatched)))
    {
      if (!isblock)
	relocate_one (file, reloc, rel,
		      cu->head->address_size, valuep, where,
		      reloc_target_loc (opcode), NULL);
      else
	wr_error (where, ": relocation relocates a length field.\n");
    }
  if (isblock)
    {
      uint64_t off_block_end = read_ctx_get_offset (ctx) + init_off - 1;
      relocation_next (reloc, off_block_end, where, skip_ok);
    }

  return true;
}

bool
check_location_expression (struct elf_file *file,
			   struct read_ctx *parent_ctx,
			   struct cu *cu,
			   uint64_t init_off,
			   struct relocation_data *reloc,
			   size_t length,
			   struct where *wh)
{
  struct read_ctx ctx;
  if (!read_ctx_init_sub (&ctx, parent_ctx, parent_ctx->ptr,
			  parent_ctx->ptr + length))
    {
      wr_error (wh, PRI_NOT_ENOUGH, "location expression");
      return false;
    }

  struct ref_record oprefs;
  WIPE (oprefs);

  struct addr_record opaddrs;
  WIPE (opaddrs);

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec_locexpr, wh);
      uint64_t opcode_off = read_ctx_get_offset (&ctx) + init_off;
      where_reset_1 (&where, opcode_off);
      addr_record_add (&opaddrs, opcode_off);

      uint8_t opcode;
      if (!read_ctx_read_ubyte (&ctx, &opcode))
	{
	  wr_error (&where, ": can't read opcode.\n");
	  break;
	}

      uint8_t op1, op2;
      if (!get_location_opcode_operands (opcode, &op1, &op2))
	{
	  wr_error (&where, ": can't decode opcode \"%s\".\n",
		    dwarf_locexpr_opcode_string (opcode));
	  break;
	}

      uint64_t value1, value2;
      if (!op_read_form (file, &ctx, cu, init_off, reloc,
			 opcode, op1, &value1, "1st operand", &where)
	  || !op_read_form (file, &ctx, cu, init_off, reloc,
			    opcode, op2, &value2, "2st operand", &where))
	goto out;

      switch (opcode)
	{
	case DW_OP_bra:
	case DW_OP_skip:
	  {
	    int16_t skip = (uint16_t)value1;

	    if (skip == 0)
	      wr_message (mc_loc | mc_acc_bloat | mc_impact_3, &where,
			  ": %s with skip 0.\n",
			  dwarf_locexpr_opcode_string (opcode));
	    else if (skip > 0 && !read_ctx_need_data (&ctx, (size_t)skip))
	      wr_error (&where, ": %s branches out of location expression.\n",
			dwarf_locexpr_opcode_string (opcode));
	    /* Compare with the offset after the two-byte skip value.  */
	    else if (skip < 0 && ((uint64_t)-skip) > read_ctx_get_offset (&ctx))
	      wr_error (&where,
			": %s branches before the beginning of location expression.\n",
			dwarf_locexpr_opcode_string (opcode));
	    else
	      {
		uint64_t off_after = read_ctx_get_offset (&ctx) + init_off;
		ref_record_add (&oprefs, off_after + skip, &where);
	      }

	    break;
	  }

	case DW_OP_const8u:
	case DW_OP_const8s:
	  if (cu->head->address_size == 4)
	    wr_error (&where, ": %s on 32-bit machine.\n",
		      dwarf_locexpr_opcode_string (opcode));
	  break;

	default:
	  if (cu->head->address_size == 4
	      && (opcode == DW_OP_constu
		  || opcode == DW_OP_consts
		  || opcode == DW_OP_deref_size
		  || opcode == DW_OP_plus_uconst)
	      && (value1 > (uint64_t)(uint32_t)-1))
	    wr_message (mc_loc | mc_acc_bloat | mc_impact_3, &where,
			": %s with operand %#" PRIx64 " on 32-bit machine.\n",
			dwarf_locexpr_opcode_string (opcode), value1);
	};
    }

 out:
  for (size_t i = 0; i < oprefs.size; ++i)
    {
      struct ref *ref = oprefs.refs + i;
      if (!addr_record_has_addr (&opaddrs, ref->addr))
	wr_error (&ref->who,
		  ": unresolved reference to opcode at %#" PRIx64 ".\n",
		  ref->addr);
    }

  addr_record_free (&opaddrs);
  ref_record_free (&oprefs);

  return true;
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
