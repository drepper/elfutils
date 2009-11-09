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

/* True if coverage analysis of .debug_ranges vs. ELF sections should
   be done.  */
static const bool do_range_coverage = false;

static bool
check_category (enum message_category cat)
{
  return message_accept (&warning_criteria, cat);
}

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64


static struct cu *cu_find_cu (struct cu *cu_chain, uint64_t offset);

static bool check_location_expression (struct elf_file *file,
				       struct read_ctx *ctx,
				       struct cu *cu,
				       uint64_t init_off,
				       struct relocation_data *reloc,
				       size_t length,
				       struct where *wh);

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

static bool
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
   type-casted int64_t.  WHAT and WHERE describe error message and
   context for LEB128 loading.  */
static bool
read_ctx_read_form (struct read_ctx *ctx, struct cu *cu, uint8_t form,
		    uint64_t *valuep, struct where *where, const char *what)
{
  switch (form)
    {
    case DW_FORM_addr:
      return read_ctx_read_offset (ctx, cu->address_size == 8, valuep);
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

  return false;
}

bool
attrib_form_valid (uint64_t form)
{
  return form > 0 && form <= DW_FORM_ref_sig8;
}

int
check_sibling_form (uint64_t form)
{
  switch (form)
    {
    case DW_FORM_indirect:
      /* Tolerate this in abbrev loading, even during the DIE loading.
	 We check that dereferenced indirect form yields valid form.  */
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      return 0;

    case DW_FORM_ref_addr:
      return -1;

    default:
      return -2;
    };
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
found_hole (uint64_t start, uint64_t length, void *data)
{
  struct hole_info *info = (struct hole_info *)data;
  bool all_zeroes = true;
  for (uint64_t i = start; i < start + length; ++i)
    if (((char*)info->data)[i] != 0)
      {
	all_zeroes = false;
	break;
      }

  uint64_t end = start + length;
  if (all_zeroes)
    {
      /* Zero padding is valid, if it aligns on the bounds of
	 info->align bytes, and is not excessive.  */
      if (!(info->align != 0 && info->align != 1
	    && (end % info->align == 0) && (start % 4 != 0)
	    && (length < info->align)))
	wr_message_padding_0 (info->category, &WHERE (info->section, NULL),
			      start, end);
    }
  else
    /* XXX: This actually lies when the unreferenced portion is
       composed of sequences of zeroes and non-zeroes.  */
    wr_message_padding_n0 (info->category, &WHERE (info->section, NULL),
			   start, end);

  return true;
}

/* begin is inclusive, end is exclusive. */
bool
coverage_map_found_hole (uint64_t begin, uint64_t end,
			 struct section_coverage *sco, void *user)
{
  struct coverage_map_hole_info *info = (struct coverage_map_hole_info *)user;

  struct where where = WHERE (info->info.section, NULL);
  const char *scnname = sco->sec->name;

  struct sec *sec = sco->sec;
  GElf_Xword align = sec->shdr.sh_addralign;

  /* We don't expect some sections to be covered.  But if they
     are at least partially covered, we expect the same
     coverage criteria as for .text.  */
  if (!sco->hit
      && ((sco->sec->shdr.sh_flags & SHF_EXECINSTR) == 0
	  || strcmp (scnname, ".init") == 0
	  || strcmp (scnname, ".fini") == 0
	  || strcmp (scnname, ".plt") == 0))
    return true;

  /* For REL files, don't print addresses mangled by our layout.  */
  uint64_t base = info->elf->ehdr.e_type == ET_REL ? 0 : sco->sec->shdr.sh_addr;

  /* If the hole is filled with NUL bytes, don't report it.  But if we
     get stripped debuginfo file, the data may not be available.  In
     that case don't report the hole, if it seems to be alignment
     padding.  */
  if (sco->sec->data->d_buf != NULL)
    {
      bool zeroes = true;
      for (uint64_t j = begin; j < end; ++j)
	if (((char *)sco->sec->data->d_buf)[j] != 0)
	  {
	    zeroes = false;
	    break;
	  }
      if (zeroes)
	return true;
    }
  else if (necessary_alignment (base + begin, end - begin, align))
    return true;

  char buf[128];
  wr_message (info->info.category | mc_acc_suboptimal | mc_impact_4, &where,
	      ": addresses %s of section %s are not covered.\n",
	      range_fmt (buf, sizeof buf, begin + base, end + base), scnname);
  return true;
}


void
section_coverage_init (struct section_coverage *sco,
		       struct sec *sec, bool warn)
{
  assert (sco != NULL);
  assert (sec != NULL);

  sco->sec = sec;
  WIPE (sco->cov);
  sco->hit = false;
  sco->warn = warn;
}

bool
coverage_map_init (struct coverage_map *coverage_map,
		   struct elf_file *elf,
		   Elf64_Xword mask,
		   Elf64_Xword warn_mask,
		   bool allow_overlap)
{
  assert (coverage_map != NULL);
  assert (elf != NULL);

  WIPE (*coverage_map);
  coverage_map->elf = elf;
  coverage_map->allow_overlap = allow_overlap;

  for (size_t i = 1; i < elf->size; ++i)
    {
      struct sec *sec = elf->sec + i;

      bool normal = (sec->shdr.sh_flags & mask) == mask;
      bool warn = (sec->shdr.sh_flags & warn_mask) == warn_mask;
      if (normal || warn)
	{
	  REALLOC (coverage_map, scos);
	  section_coverage_init
	    (coverage_map->scos + coverage_map->size++, sec, !normal);
	}
    }

  return true;
}

void
coverage_map_add (struct coverage_map *coverage_map,
		  uint64_t address,
		  uint64_t length,
		  struct where *where,
		  enum message_category cat)
{
  bool found = false;
  bool crosses_boundary = false;
  bool overlap = false;
  uint64_t end = address + length;
  char buf[128]; // for messages

  /* This is for analyzing how much of the current range falls into
     sections in coverage map.  Whatever is left uncovered doesn't
     fall anywhere and is reported.  */
  struct coverage range_cov;
  WIPE (range_cov);

  for (size_t i = 0; i < coverage_map->size; ++i)
    {
      struct section_coverage *sco = coverage_map->scos + i;
      GElf_Shdr *shdr = &sco->sec->shdr;
      struct coverage *cov = &sco->cov;

      Elf64_Addr s_end = shdr->sh_addr + shdr->sh_size;
      if (end <= shdr->sh_addr || address >= s_end)
	/* no overlap */
	continue;

      if (found && !crosses_boundary)
	{
	  /* While probably not an error, it's very suspicious.  */
	  wr_message (cat | mc_impact_2, where,
		      ": the range %s crosses section boundaries.\n",
		      range_fmt (buf, sizeof buf, address, end));
	  crosses_boundary = true;
	}

      found = true;

      if (length == 0)
	/* Empty range.  That means no actual coverage, and we can
	   also be sure that there are no more sections that this one
	   falls into.  */
	break;

      uint64_t cov_begin
	= address < shdr->sh_addr ? 0 : address - shdr->sh_addr;
      uint64_t cov_end
	= end < s_end ? end - shdr->sh_addr : shdr->sh_size;
      assert (cov_begin < cov_end);

      uint64_t r_delta = shdr->sh_addr - address;
      uint64_t r_cov_begin = cov_begin + r_delta;
      uint64_t r_cov_end = cov_end + r_delta;

      if (!overlap && !coverage_map->allow_overlap
	  && coverage_is_overlap (cov, cov_begin, cov_end - cov_begin))
	{
	  /* Not a show stopper, this shouldn't derail high-level.  */
	  wr_message (cat | mc_impact_2 | mc_error, where,
		      ": the range %s overlaps with another one.\n",
		      range_fmt (buf, sizeof buf, address, end));
	  overlap = true;
	}

      if (sco->warn)
	wr_message (cat | mc_impact_2, where,
		    ": the range %s covers section %s.\n",
		    range_fmt (buf, sizeof buf, address, end), sco->sec->name);

      /* Section coverage... */
      coverage_add (cov, cov_begin, cov_end - cov_begin);
      sco->hit = true;

      /* And range coverage... */
      coverage_add (&range_cov, r_cov_begin, r_cov_end - r_cov_begin);
    }

  if (!found)
    /* Not a show stopper.  */
    wr_error (where,
	      ": couldn't find a section that the range %s covers.\n",
	      range_fmt (buf, sizeof buf, address, end));
  else if (length > 0)
    {
      bool range_hole (uint64_t h_start, uint64_t h_length,
		       void *user __attribute__ ((unused)))
      {
	char buf2[128];
	assert (h_length != 0);
	wr_error (where,
		  ": portion %s of the range %s "
		  "doesn't fall into any ALLOC section.\n",
		  range_fmt (buf, sizeof buf,
			     h_start + address, h_start + address + h_length),
		  range_fmt (buf2, sizeof buf2, address, end));
	return true;
      }
      coverage_find_holes (&range_cov, 0, length, range_hole, NULL);
    }

  coverage_free (&range_cov);
}

bool
coverage_map_find_holes (struct coverage_map *coverage_map,
			 bool (*cb) (uint64_t begin, uint64_t end,
				     struct section_coverage *, void *),
			 void *user)
{
  for (size_t i = 0; i < coverage_map->size; ++i)
    {
      struct section_coverage *sco = coverage_map->scos + i;

      bool wrap_cb (uint64_t h_start, uint64_t h_length, void *h_user)
      {
	return cb (h_start, h_start + h_length, sco, h_user);
      }

      if (!coverage_find_holes (&sco->cov, 0, sco->sec->shdr.sh_size,
				wrap_cb, user))
	return false;
    }

  return true;
}

void
coverage_map_free (struct coverage_map *coverage_map)
{
  for (size_t i = 0; i < coverage_map->size; ++i)
    coverage_free (&coverage_map->scos[i].cov);
  free (coverage_map->scos);
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
    if (it->offset == offset)
      return it;
  return NULL;
}


static bool
check_die_references (struct cu *cu,
		      struct ref_record *die_refs)
{
  bool retval = true;
  for (size_t i = 0; i < die_refs->size; ++i)
    {
      struct ref *ref = die_refs->refs + i;
      if (!addr_record_has_addr (&cu->die_addrs, ref->addr))
	{
	  wr_error (&ref->who,
		    ": unresolved reference to " PRI_DIE ".\n", ref->addr);
	  retval = false;
	}
    }
  return retval;
}

static bool
check_global_die_references (struct cu *cu_chain)
{
  bool retval = true;
  for (struct cu *it = cu_chain; it != NULL; it = it->next)
    for (size_t i = 0; i < it->die_refs.size; ++i)
      {
	struct ref *ref = it->die_refs.refs + i;
	struct cu *ref_cu = NULL;
	for (struct cu *jt = cu_chain; jt != NULL; jt = jt->next)
	  if (addr_record_has_addr (&jt->die_addrs, ref->addr))
	    {
	      ref_cu = jt;
	      break;
	    }

	if (ref_cu == NULL)
	  {
	    wr_error (&ref->who,
		      ": unresolved (non-CU-local) reference to " PRI_DIE ".\n",
		      ref->addr);
	    retval = false;
	  }
	else if (ref_cu == it)
	  /* This is technically not a problem, so long as the
	     reference is valid, which it is.  But warn about this
	     anyway, perhaps local reference could be formed on fewer
	     number of bytes.  */
	  wr_message (mc_impact_2 | mc_acc_suboptimal | mc_die_rel,
		      &ref->who,
		      ": local reference to " PRI_DIE " formed as global.\n",
		      ref->addr);
      }

  return retval;
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
		    struct where *wh)
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
reloc_target (uint8_t form, struct abbrev_attrib *at)
{
  switch (form)
    {
    case DW_FORM_strp:
      return sec_str;

    case DW_FORM_addr:

      switch (at->name)
	{
	case DW_AT_low_pc:
	case DW_AT_high_pc:
	case DW_AT_entry_pc:
	  return rel_exec;

	case DW_AT_const_value:
	  /* Appears in some kernel modules.  It's not allowed by the
	     standard, but leave that for high-level checks.  */
	  return rel_address;
	};

      break;

    case DW_FORM_ref_addr:
      return sec_info;

    case DW_FORM_data1:
    case DW_FORM_data2:
      /* While these are technically legal, they are never used in
	 DWARF sections.  So better mark them as illegal, and have
	 dwarflint flag them.  */
      return sec_invalid;

    case DW_FORM_data4:
    case DW_FORM_data8:

      switch (at->name)
	{
	case DW_AT_stmt_list:
	  return sec_line;

	case DW_AT_location:
	case DW_AT_string_length:
	case DW_AT_return_addr:
	case DW_AT_data_member_location:
	case DW_AT_frame_base:
	case DW_AT_segment:
	case DW_AT_static_link:
	case DW_AT_use_location:
	case DW_AT_vtable_elem_location:
	  return sec_loc;

	case DW_AT_mac_info:
	  return sec_mac;

	case DW_AT_ranges:
	  return sec_ranges;
	}

      break;

    case DW_FORM_string:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
      /* Shouldn't be relocated.  */
      return sec_invalid;

    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_flag:
    case DW_FORM_flag_present:
    case DW_FORM_ref_udata:
      assert (!"Can't be relocated!");

    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
      assert (!"Should be handled specially!");
    };

  printf ("XXX don't know how to handle form=%s, at=%s\n",
	  dwarf_form_string (form), dwarf_attr_string (at->name));

  return rel_value;
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

static void
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

/*
  Returns:
    -1 in case of error
    +0 in case of no error, but the chain only consisted of a
       terminating zero die.
    +1 in case some dies were actually loaded
 */
static int
read_die_chain (struct elf_file *file,
		struct read_ctx *ctx,
		struct cu *cu,
		struct abbrev_table *abbrevs,
		Elf_Data *strings,
		struct ref_record *local_die_refs,
		struct coverage *strings_coverage,
		struct relocation_data *reloc,
		struct cu_coverage *cu_coverage)
{
  bool got_die = false;
  uint64_t sibling_addr = 0;
  uint64_t die_off, prev_die_off = 0;
  struct abbrev *abbrev = NULL;
  struct abbrev *prev_abbrev = NULL;
  struct where where = WHERE (sec_info, NULL);

  while (!read_ctx_eof (ctx))
    {
      where = cu->where;
      die_off = read_ctx_get_offset (ctx);
      /* Shift reported DIE offset by CU offset, to match the way
	 readelf reports DIEs.  */
      where_reset_2 (&where, die_off + cu->offset);

      uint64_t abbr_code;

      if (!checked_read_uleb128 (ctx, &abbr_code, &where, "abbrev code"))
	return -1;

#define DEF_PREV_WHERE							\
      struct where prev_where = where;					\
      where_reset_2 (&prev_where, prev_die_off + cu->offset)

      /* Check sibling value advertised last time through the loop.  */
      if (sibling_addr != 0)
	{
	  if (abbr_code == 0)
	    wr_error (&where,
		      ": is the last sibling in chain, "
		      "but has a DW_AT_sibling attribute.\n");
	  else if (sibling_addr != die_off)
	    {
	      DEF_PREV_WHERE;
	      wr_error (&prev_where,
			": This DIE claims that its sibling is 0x%"
			PRIx64 ", but it's actually 0x%" PRIx64 ".\n",
			sibling_addr, die_off);
	    }
	  sibling_addr = 0;
	}
      else if (abbr_code != 0
	       && abbrev != NULL && abbrev->has_children)
	{
	  /* Even if it has children, the DIE can't have a sibling
	     attribute if it's the last DIE in chain.  That's the
	     reason we can't simply check this when loading
	     abbrevs.  */
	  DEF_PREV_WHERE;
	  wr_message (mc_die_rel | mc_acc_suboptimal | mc_impact_4, &prev_where,
		      ": This DIE had children, but no DW_AT_sibling attribute.\n");
	}
#undef DEF_PREV_WHERE

      prev_die_off = die_off;

      /* The section ended.  */
      if (abbr_code == 0)
	break;
      if (read_ctx_eof (ctx))
	{
	  wr_error (&where, ": DIE chain not terminated with DIE with zero abbrev code.\n");
	  break;
	}

      prev_die_off = die_off;
      got_die = true;
      if (dump_die_offsets)
	fprintf (stderr, "%s: abbrev %" PRId64 "\n",
		 where_fmt (&where, NULL), abbr_code);

      /* Find the abbrev matching the code.  */
      prev_abbrev = abbrev;
      abbrev = abbrev_table_find_abbrev (abbrevs, abbr_code);
      if (abbrev == NULL)
	{
	  wr_error (&where,
		    ": abbrev section at 0x%" PRIx64
		    " doesn't contain code %" PRIu64 ".\n",
		    abbrevs->offset, abbr_code);
	  return -1;
	}
      abbrev->used = true;

      addr_record_add (&cu->die_addrs, cu->offset + die_off);

      uint64_t low_pc = (uint64_t)-1, high_pc = (uint64_t)-1;
      bool low_pc_relocated = false, high_pc_relocated = false;
      GElf_Sym low_pc_symbol_mem, *low_pc_symbol = &low_pc_symbol_mem;
      GElf_Sym high_pc_symbol_mem, *high_pc_symbol = &high_pc_symbol_mem;

      /* Attribute values.  */
      for (struct abbrev_attrib *it = abbrev->attribs;
	   it->name != 0; ++it)
	{
	  where.ref = &it->where;

	  uint8_t form = it->form;
	  bool indirect = form == DW_FORM_indirect;
	  if (indirect)
	    {
	      uint64_t value;
	      if (!checked_read_uleb128 (ctx, &value, &where,
					 "indirect attribute form"))
		return -1;

	      if (!attrib_form_valid (value))
		{
		  wr_error (&where,
			    ": invalid indirect form 0x%" PRIx64 ".\n", value);
		  return -1;
		}
	      form = value;

	      if (it->name == DW_AT_sibling)
		switch (check_sibling_form (form))
		  {
		  case -1:
		    wr_message (mc_die_rel | mc_impact_2, &where,
				": DW_AT_sibling attribute with (indirect) form DW_FORM_ref_addr.\n");
		    break;

		  case -2:
		    wr_error (&where,
			      ": DW_AT_sibling attribute with non-reference (indirect) form \"%s\".\n",
			      dwarf_form_string (value));
		  };
	    }

	  void (*value_check_cb) (uint64_t, struct where *) = NULL;

	  /* For checking lineptr, rangeptr, locptr.  */
	  bool check_someptr = false;
	  enum message_category extra_mc = mc_none;

	  /* Callback for local DIE references.  */
	  void check_die_ref_local (uint64_t addr, struct where *who)
	  {
	    assert (ctx->end > ctx->begin);
	    if (addr > (uint64_t)(ctx->end - ctx->begin))
	      {
		wr_error (&where,
			  ": invalid reference outside the CU: 0x%" PRIx64 ".\n",
			  addr);
		return;
	      }

	    if (local_die_refs != NULL)
	      /* Address holds a CU-local reference, so add CU offset
		 to turn it into section offset.  */
	      ref_record_add (local_die_refs, addr += cu->offset, who);
	  }

	  /* Callback for global DIE references.  */
	  void check_die_ref_global (uint64_t addr, struct where *who)
	  {
	    ref_record_add (&cu->die_refs, addr, who);
	  }

	  /* Callback for strp values.  */
	  void check_strp (uint64_t addr, struct where *who)
	  {
	    if (strings == NULL)
	      wr_error (who, ": strp attribute, but no .debug_str data.\n");
	    else if (addr >= strings->d_size)
	      wr_error (who,
			": Invalid offset outside .debug_str: 0x%" PRIx64 ".\n",
			addr);
	    else
	      {
		/* Record used part of .debug_str.  */
		const char *startp = (const char *)strings->d_buf + addr;
		const char *data_end = strings->d_buf + strings->d_size;
		const char *strp = startp;
		while (strp < data_end && *strp != 0)
		  ++strp;
		if (strp == data_end)
		  wr_error (who,
			    ": String at .debug_str: 0x%" PRIx64
			    " is not zero-terminated.\n", addr);

		if (strings_coverage != NULL)
		  coverage_add (strings_coverage, addr, strp - startp + 1);
	      }
	  }

	  /* Callback for rangeptr values.  */
	  void check_rangeptr (uint64_t value, struct where *who)
	  {
	    if ((value % cu->address_size) != 0)
	      wr_message (mc_ranges | mc_impact_2, who,
			  ": rangeptr value %#" PRIx64
			  " not aligned to CU address size.\n", value);
	    cu_coverage->need_ranges = true;
	    ref_record_add (&cu->range_refs, value, who);
	  }

	  /* Callback for lineptr values.  */
	  void check_lineptr (uint64_t value, struct where *who)
	  {
	    ref_record_add (&cu->line_refs, value, who);
	  }

	  /* Callback for locptr values.  */
	  void check_locptr (uint64_t value, struct where *who)
	  {
	    ref_record_add (&cu->loc_refs, value, who);
	  }

	  uint64_t ctx_offset = read_ctx_get_offset (ctx) + cu->offset;
	  bool type_is_rel = file->ehdr.e_type == ET_REL;

	  /* Attribute value.  */
	  uint64_t value = 0;

	  /* Whether the value should be relocated first.  Note that
	     relocations are really required only in REL files, so
	     missing relocations are not warned on even with
	     rel_require, unless type_is_rel.  */
	  enum
	  {
	    rel_no,		// don't allow a relocation
	    rel_require,	// require a relocation
	    rel_nonzero,	// require a relocation if value != 0
	  } relocate = rel_no;
	  size_t width = 0;

	  /* Point to variable that you want to copy relocated value
	     to.  */
	  uint64_t *valuep = NULL;

	  /* Point to variable that you want set to `true' in case the
	     value was relocated.  */
	  bool *relocatedp = NULL;

	  /* Point to variable that you want set to symbol that the
	     relocation was made against.  */
	  GElf_Sym **symbolp = NULL;

	  /* Setup locptr checking.  */
	  if (is_location_attrib (it->name))
	    {
	      switch (form)
		{
		case DW_FORM_data8:
		  if (cu->offset_size == 4)
		    wr_error (&where,
			      ": location attribute with form \"%s\" in 32-bit CU.\n",
			      dwarf_form_string (form));
		  /* fall-through */

		case DW_FORM_data4:
		case DW_FORM_sec_offset:
		  value_check_cb = check_locptr;
		  extra_mc = mc_loc;
		  check_someptr = true;
		  break;

		case DW_FORM_block1:
		case DW_FORM_block2:
		case DW_FORM_block4:
		case DW_FORM_block:
		  break;

		default:
		  /* Only print error if it's indirect.  Otherwise we
		     gave diagnostic during abbrev loading.  */
		  if (indirect)
		    wr_error (&where,
			      ": location attribute with invalid (indirect) form \"%s\".\n",
			      dwarf_form_string (form));
		};
	    }
	  /* Setup rangeptr or lineptr checking.  */
	  else if (it->name == DW_AT_ranges
		   || it->name == DW_AT_stmt_list)
	    switch (form)
	      {
	      case DW_FORM_data8:
		if (cu->offset_size == 4)
		  wr_error (&where,
			    ": %s with form DW_FORM_data8 in 32-bit CU.\n",
			    dwarf_attr_string (it->name));
		/* fall-through */

	      case DW_FORM_data4:
	      case DW_FORM_sec_offset:
		check_someptr = true;
		if (it->name == DW_AT_ranges)
		  {
		    value_check_cb = check_rangeptr;
		    extra_mc = mc_ranges;
		  }
		else
		  {
		    assert (it->name == DW_AT_stmt_list);
		    value_check_cb = check_lineptr;
		    extra_mc = mc_line;
		  }
		break;

	      default:
		/* Only print error if it's indirect.  Otherwise we
		   gave diagnostic during abbrev loading.  */
		if (indirect)
		  wr_error (&where,
			    ": %s with invalid (indirect) form \"%s\".\n",
			    dwarf_attr_string (it->name),
			    dwarf_form_string (form));
	      }
	  /* Setup low_pc and high_pc checking.  */
	  else if (it->name == DW_AT_low_pc)
	    {
	      relocatedp = &low_pc_relocated;
	      symbolp = &low_pc_symbol;
	      valuep = &low_pc;
	    }
	  else if (it->name == DW_AT_high_pc)
	    {
	      relocatedp = &high_pc_relocated;
	      symbolp = &high_pc_symbol;
	      valuep = &high_pc;
	    }

	  /* Load attribute value and setup per-form checking.  */
	  switch (form)
	    {
	    case DW_FORM_strp:
	      value_check_cb = check_strp;
	    case DW_FORM_sec_offset:
	      if (!read_ctx_read_offset (ctx, cu->offset_size == 8, &value))
		{
		cant_read:
		  wr_error (&where, ": can't read value of attribute %s.\n",
			    dwarf_attr_string (it->name));
		  return -1;
		}

	      relocate = rel_require;
	      width = cu->offset_size;
	      break;

	    case DW_FORM_string:
	      if (!read_ctx_read_str (ctx))
		goto cant_read;
	      break;

	    case DW_FORM_ref_addr:
	      value_check_cb = check_die_ref_global;
	      width = cu->offset_size;

	      if (cu->version == 2)
	    case DW_FORM_addr:
		width = cu->address_size;

	      if (!read_ctx_read_offset (ctx, width == 8, &value))
		goto cant_read;

	      /* In non-rel files, neither addr, nor ref_addr /need/
		 a relocation.  */
	      relocate = rel_nonzero;
	      break;

	    case DW_FORM_ref_udata:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_udata:
	      if (!checked_read_uleb128 (ctx, &value, &where,
					 "attribute value"))
		return -1;
	      break;

	    case DW_FORM_flag_present:
	      value = 1;
	      break;

	    case DW_FORM_ref1:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_flag:
	    case DW_FORM_data1:
	      if (!read_ctx_read_var (ctx, 1, &value))
		goto cant_read;
	      break;

	    case DW_FORM_ref2:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_data2:
	      if (!read_ctx_read_var (ctx, 2, &value))
		goto cant_read;
	      break;

	    case DW_FORM_data4:
	      if (check_someptr)
		{
		  relocate = rel_require;
		  width = 4;
		}
	      if (false)
	    case DW_FORM_ref4:
		value_check_cb = check_die_ref_local;
	      if (!read_ctx_read_var (ctx, 4, &value))
		goto cant_read;
	      break;

	    case DW_FORM_data8:
	      if (check_someptr)
		{
		  relocate = rel_require;
		  width = 8;
		}
	      if (false)
	    case DW_FORM_ref8:
		value_check_cb = check_die_ref_local;
	      if (!read_ctx_read_8ubyte (ctx, &value))
		goto cant_read;
	      break;

	    case DW_FORM_sdata:
	      {
		int64_t value64;
		if (!checked_read_sleb128 (ctx, &value64, &where,
					   "attribute value"))
		  return -1;
		value = (uint64_t) value64;
		break;
	      }

	    case DW_FORM_block:
	      {
		uint64_t length;

		if (false)
	    case DW_FORM_block1:
		  width = 1;

		if (false)
	    case DW_FORM_block2:
		  width = 2;

		if (false)
	    case DW_FORM_block4:
		  width = 4;

		if (width == 0)
		  {
		    if (!checked_read_uleb128 (ctx, &length, &where,
					       "attribute value"))
		      return -1;
		  }
		else if (!read_ctx_read_var (ctx, width, &length))
		  goto cant_read;

		if (is_location_attrib (it->name))
		  {
		    uint64_t expr_start
		      = cu->offset + read_ctx_get_offset (ctx);
		    if (!check_location_expression (file, ctx, cu, expr_start,
						    reloc, length, &where))
		      return -1;
		  }
		else
		  /* xxx really skip_mismatched?  We just don't know
		     how to process these...  */
		  relocation_skip (reloc,
				   read_ctx_get_offset (ctx) + length,
				   &where, skip_mismatched);

		if (!read_ctx_skip (ctx, length))
		  goto cant_read;
		break;
	      }

	    case DW_FORM_indirect:
	      wr_error (&where, ": indirect form is again indirect.\n");
	      return -1;

	    default:
	      wr_error (&where,
			": internal error: unhandled form 0x%x.\n", form);
	    }

	  /* Relocate the value if appropriate.  */
	  struct relocation *rel;
	  if ((rel = relocation_next (reloc, ctx_offset,
				      &where, skip_mismatched)))
	    {
	      if (relocate == rel_no)
		wr_message (mc_impact_4 | mc_die_other | mc_reloc | extra_mc,
			    &where, ": unexpected relocation of %s.\n",
			    dwarf_form_string (form));

	      relocate_one (file, reloc, rel, width, &value, &where,
			    reloc_target (form, it), symbolp);

	      if (relocatedp != NULL)
		*relocatedp = true;
	    }
	  else
	    {
	      if (symbolp != NULL)
		WIPE (*symbolp);
	      if (type_is_rel
		  && (relocate == rel_require
		      || (relocate == rel_nonzero
			  && value != 0)))
		wr_message (mc_impact_2 | mc_die_other | mc_reloc | extra_mc,
			    &where, PRI_LACK_RELOCATION,
			    dwarf_form_string (form));
	    }

	  /* Dispatch value checking.  */
	  if (it->name == DW_AT_sibling)
	    {
	      /* Full-blown DIE reference checking is too heavy-weight
		 and not practical (error messages wise) for checking
		 siblings.  */
	      assert (value_check_cb == check_die_ref_local
		      || value_check_cb == check_die_ref_global);
	      valuep = &sibling_addr;
	    }
	  else if (value_check_cb != NULL)
	    value_check_cb (value, &where);

	  /* Store the relocated value.  Note valuep may point to
	     low_pc or high_pc.  */
	  if (valuep != NULL)
	    *valuep = value;

	  /* Check PC coverage.  */
	  if (abbrev->tag == DW_TAG_compile_unit
	      || abbrev->tag == DW_TAG_partial_unit)
	    {
	      if (it->name == DW_AT_low_pc)
		cu->low_pc = value;

	      if (low_pc != (uint64_t)-1 && high_pc != (uint64_t)-1)
		coverage_add (&cu_coverage->cov, low_pc, high_pc - low_pc);
	    }
	}
      where.ref = NULL;

      if (high_pc != (uint64_t)-1 && low_pc != (uint64_t)-1)
	{
	  if (high_pc_relocated != low_pc_relocated)
	    wr_message (mc_die_other | mc_impact_2 | mc_reloc, &where,
			": only one of DW_AT_low_pc and DW_AT_high_pc is relocated.\n");
	  else
	    check_range_relocations (mc_die_other, &where,
				     file,
				     low_pc_symbol, high_pc_symbol,
				     "DW_AT_low_pc and DW_AT_high_pc");
	}

      where.ref = &abbrev->where;

      if (abbrev->has_children)
	{
	  int st = read_die_chain (file, ctx, cu, abbrevs, strings,
				   local_die_refs,
				   strings_coverage, reloc,
				   cu_coverage);
	  if (st == -1)
	    return -1;
	  else if (st == 0)
	    wr_message (mc_impact_3 | mc_acc_suboptimal | mc_die_rel,
			&where,
			": abbrev has_children, but the chain was empty.\n");
	}
    }

  if (sibling_addr != 0)
    wr_error (&where,
	      ": this DIE should have had its sibling at 0x%"
	      PRIx64 ", but the DIE chain ended.\n", sibling_addr);

  return got_die ? 1 : 0;
}

static bool
read_address_size (struct elf_file *file,
		   struct read_ctx *ctx,
		   uint8_t *address_sizep,
		   struct where *where)
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
      address_size = file->addr_64 ? 8 : 4;
    }
  else if ((address_size == 8) != file->addr_64)
    /* Keep going, we may still be able to parse it.  */
    wr_error (where,
	      ": CU reports address size of %d in %d-bit ELF.\n",
	      address_size, file->addr_64 ? 64 : 32);

  *address_sizep = address_size;
  return true;
}

static bool
check_cu_structural (struct elf_file *file,
		     struct read_ctx *ctx,
		     struct cu *const cu,
		     struct abbrev_table *abbrev_chain,
		     Elf_Data *strings,
		     struct coverage *strings_coverage,
		     struct relocation_data *reloc,
		     struct cu_coverage *cu_coverage)
{
  if (dump_die_offsets)
    fprintf (stderr, "%s: CU starts\n", where_fmt (&cu->where, NULL));
  bool retval = true;

  /* Version.  */
  uint16_t version;
  if (!read_ctx_read_2ubyte (ctx, &version))
    {
      wr_error (&cu->where, ": can't read version.\n");
      return false;
    }
  if (!supported_version (version, 2, &cu->where, 2, 3))
    return false;
  if (version == 2 && cu->offset_size == 8)
    /* Keep going.  It's a standard violation, but we may still be
       able to read the unit under consideration and do high-level
       checks.  */
    wr_error (&cu->where, ": invalid 64-bit unit in DWARF 2 format.\n");
  cu->version = version;

  /* Abbrev offset.  */
  uint64_t abbrev_offset;
  uint64_t ctx_offset = read_ctx_get_offset (ctx) + cu->offset;
  if (!read_ctx_read_offset (ctx, cu->offset_size == 8, &abbrev_offset))
    {
      wr_error (&cu->where, ": can't read abbrev offset.\n");
      return false;
    }

  struct relocation *rel
    = relocation_next (reloc, ctx_offset, &cu->where, skip_mismatched);
  if (rel != NULL)
    relocate_one (file, reloc, rel, cu->offset_size,
		  &abbrev_offset, &cu->where, sec_abbrev, NULL);
  else if (file->ehdr.e_type == ET_REL)
    wr_message (mc_impact_2 | mc_info | mc_reloc, &cu->where,
		PRI_LACK_RELOCATION, "abbrev offset");

  /* Address size.  */
  {
    uint8_t address_size;
    if (!read_address_size (file, ctx, &address_size, &cu->where))
      return false;
    cu->address_size = address_size;
  }

  /* Look up Abbrev table for this CU.  */
  struct abbrev_table *abbrevs = abbrev_chain;
  for (; abbrevs != NULL; abbrevs = abbrevs->next)
    if (abbrevs->offset == abbrev_offset)
      break;

  if (abbrevs == NULL)
    {
      wr_error (&cu->where,
		": couldn't find abbrev section with offset %" PRId64 ".\n",
		abbrev_offset);
      return false;
    }

  abbrevs->used = true;

  /* Read DIEs.  */
  struct ref_record local_die_refs;
  WIPE (local_die_refs);

  cu->cudie_offset = read_ctx_get_offset (ctx) + cu->offset;
  if (read_die_chain (file, ctx, cu, abbrevs, strings,
		      &local_die_refs, strings_coverage,
		      (reloc != NULL && reloc->size > 0) ? reloc : NULL,
		      cu_coverage) < 0)
    {
      abbrevs->skip_check = true;
      retval = false;
    }
  else if (!check_die_references (cu, &local_die_refs))
    retval = false;

  ref_record_free (&local_die_refs);
  return retval;
}

struct cu *
check_info_structural (struct elf_file *file,
		       struct sec *sec,
		       struct abbrev_table *abbrev_chain,
		       Elf_Data *strings,
		       struct cu_coverage *cu_coverage)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  struct ref_record die_refs;
  WIPE (die_refs);

  struct cu *cu_chain = NULL;

  bool success = true;

  struct coverage strings_coverage_mem, *strings_coverage = NULL;
  if (strings != NULL && check_category (mc_strings))
    {
      WIPE (strings_coverage_mem);
      strings_coverage = &strings_coverage_mem;
    }

  struct relocation_data *reloc = sec->rel.size > 0 ? &sec->rel : NULL;
  while (!read_ctx_eof (&ctx))
    {
      const unsigned char *cu_begin = ctx.ptr;
      struct where where = WHERE (sec_info, NULL);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));

      struct cu *cur = xcalloc (1, sizeof (*cur));
      cur->offset = where.addr1;
      cur->next = cu_chain;
      cur->where = where;
      cur->low_pc = (uint64_t)-1;
      cu_chain = cur;

      uint32_t size32;
      uint64_t size;

      /* Reading CU header is a bit tricky, because we don't know if
	 we have run into (superfluous but allowed) zero padding.  */
      if (!read_ctx_need_data (&ctx, 4)
	  && check_zero_padding (&ctx, mc_info | mc_header, &where))
	break;

      /* CU length.  */
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (&where, ": can't read CU length.\n");
	  success = false;
	  break;
	}
      if (size32 == 0 && check_zero_padding (&ctx, mc_info | mc_header, &where))
	break;

      if (!read_size_extra (&ctx, size32, &size, &cur->offset_size, &where))
	{
	  success = false;
	  break;
	}

      if (!read_ctx_need_data (&ctx, size))
	{
	  wr_error (&where,
		    ": section doesn't have enough data"
		    " to read CU of size %" PRId64 ".\n", size);
	  ctx.ptr = ctx.end;
	  success = false;
	  break;
	}

      const unsigned char *cu_end = ctx.ptr + size;
      cur->length = cu_end - cu_begin; // Length including the length field.

      /* version + debug_abbrev_offset + address_size */
      uint64_t cu_header_size = 2 + cur->offset_size + 1;
      if (size < cu_header_size)
	{
	  wr_error (&where, ": claimed length of %" PRIx64
		    " doesn't even cover CU header.\n", size);
	  success = false;
	  break;
	}
      else
	{
	  /* Make CU context begin just before the CU length, so that DIE
	     offsets are computed correctly.  */
	  struct read_ctx cu_ctx;
	  if (!read_ctx_init_sub (&cu_ctx, &ctx, cu_begin, cu_end))
	    {
	    not_enough:
	      wr_error (&where, PRI_NOT_ENOUGH, "next CU");
	      success = false;
	      break;
	    }
	  cu_ctx.ptr = ctx.ptr;

	  if (!check_cu_structural (file, &cu_ctx, cur, abbrev_chain,
				    strings, strings_coverage, reloc,
				    cu_coverage))
	    {
	      success = false;
	      break;
	    }
	  if (cu_ctx.ptr != cu_ctx.end
	      && !check_zero_padding (&cu_ctx, mc_info, &where))
	    wr_message_padding_n0 (mc_info, &where,
				   read_ctx_get_offset (&ctx),
				   read_ctx_get_offset (&ctx) + size);
	}

      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (success)
    {
      if (ctx.ptr != ctx.end)
	/* Did we read up everything?  */
	wr_message (mc_die_other | mc_impact_4,
		    &WHERE (sec_info, NULL),
		    ": CU lengths don't exactly match Elf_Data contents.");
      else
	/* Did we consume all the relocations?  */
	relocation_skip_rest (&sec->rel, sec->id);

      /* If we managed to read up everything, now do abbrev usage
	 analysis.  */
      for (struct abbrev_table *abbrevs = abbrev_chain;
	   abbrevs != NULL; abbrevs = abbrevs->next)
	{
	  if (!abbrevs->used)
	    {
	      struct where wh = WHERE (sec_abbrev, NULL);
	      where_reset_1 (&wh, abbrevs->offset);
	      wr_message (mc_impact_4 | mc_acc_bloat | mc_abbrevs, &wh,
			  ": abbreviation table is never used.\n");
	    }
	  else if (!abbrevs->skip_check)
	    for (size_t i = 0; i < abbrevs->size; ++i)
	      if (!abbrevs->abbr[i].used)
		wr_message (mc_impact_3 | mc_acc_bloat | mc_abbrevs,
			    &abbrevs->abbr[i].where,
			    ": abbreviation is never used.\n");
	}
    }


  /* We used to check that all CUs have the same address size.  Now
     that we validate address_size of each CU against the ELF header,
     that's not necessary anymore.  */

  bool references_sound = check_global_die_references (cu_chain);
  ref_record_free (&die_refs);

  if (strings_coverage != NULL)
    {
      if (success)
	coverage_find_holes (strings_coverage, 0, strings->d_size, found_hole,
			     &((struct hole_info)
			       {sec_str, mc_strings, strings->d_buf, 0}));
      coverage_free (strings_coverage);
    }

  if (!success || !references_sound)
    {
      cu_free (cu_chain);
      cu_chain = NULL;
    }

  /* Reverse the chain, so that it's organized "naturally".  Has
     significant impact on performance when handling loc_ref and
     range_ref fields in loc/range validation.  */
  struct cu *last = NULL;
  for (struct cu *it = cu_chain; it != NULL; )
    {
      struct cu *next = it->next;
      it->next = last;
      last = it;
      it = next;
    }
  cu_chain = last;

  return cu_chain;
}

static struct coverage_map *
coverage_map_alloc_XA (struct elf_file *elf, bool allow_overlap)
{
  struct coverage_map *ret = xmalloc (sizeof (*ret));
  if (!coverage_map_init (ret, elf,
			  SHF_EXECINSTR | SHF_ALLOC,
			  SHF_ALLOC,
			  allow_overlap))
    {
      free (ret);
      return NULL;
    }
  return ret;
}

static void
coverage_map_free_XA (struct coverage_map *coverage_map)
{
  if (coverage_map != NULL)
    {
      coverage_map_free (coverage_map);
      free (coverage_map);
    }
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
      uint8_t address_size;
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
	  where.ref = &cu->where;
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
      if (cu != NULL && cu_len != cu->length)
	{
	  wr_error (&where,
		    ": the table covers length %" PRId64
		    " but CU has length %" PRId64 ".\n", cu_len, cu->length);
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
	      && !addr_record_has_addr (&cu->die_addrs, offset + cu->offset))
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

#define READ_FORM(OP, STR, PTR)						\
      do {								\
	if (OP != 0)							\
	  {								\
	    uint64_t _off = read_ctx_get_offset (&ctx) + init_off;	\
	    uint64_t *_ptr = (PTR);					\
	    if (!read_ctx_read_form (&ctx, cu, (OP),			\
				     _ptr, &where, STR " operand"))	\
	      {								\
		wr_error (&where, ": opcode \"%s\""			\
			  ": can't read " STR " operand (form \"%s\").\n", \
			  dwarf_locexpr_opcode_string (opcode),		\
			  dwarf_form_string ((OP)));			\
		goto out;						\
	      }								\
	    struct relocation *_rel;					\
	    if ((_rel = relocation_next (reloc, _off,			\
					 &where, skip_mismatched)))	\
	      relocate_one (file, reloc, _rel,				\
			    cu->address_size, _ptr, &where,		\
			    reloc_target_loc (opcode), NULL);		\
	  }								\
      } while (0)

      uint64_t value1, value2;
      READ_FORM (op1, "1st", &value1);
      READ_FORM (op2, "2st", &value2);
#undef READ_FORM

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
	      ref_record_add (&oprefs, opcode_off + skip, &where);

	    break;
	  }

	case DW_OP_const8u:
	case DW_OP_const8s:
	  if (cu->address_size == 4)
	    wr_error (&where, ": %s on 32-bit machine.\n",
		      dwarf_locexpr_opcode_string (opcode));
	  break;

	default:
	  if (cu->address_size == 4
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

static bool
check_loc_or_range_ref (struct elf_file *file,
			const struct read_ctx *parent_ctx,
			struct cu *cu,
			struct sec *sec,
			struct coverage *coverage,
			struct coverage_map *coverage_map,
			struct cu_coverage *cu_coverage,
			uint64_t addr,
			struct where *wh,
			enum message_category cat)
{
  char buf[128]; // messages

  assert (sec->id == sec_loc || sec->id == sec_ranges);
  assert (cat == mc_loc || cat == mc_ranges);
  assert ((sec->id == sec_loc) == (cat == mc_loc));
  assert (coverage != NULL);

  struct read_ctx ctx;
  read_ctx_init (&ctx, parent_ctx->data, file->other_byte_order);
  if (!read_ctx_skip (&ctx, addr))
    {
      wr_error (wh, ": invalid reference outside the section "
		"%#" PRIx64 ", size only %#tx.\n",
		addr, ctx.end - ctx.begin);
      return false;
    }

  bool retval = true;
  bool contains_locations = sec->id == sec_loc;

  if (coverage_is_covered (coverage, addr, 1))
    {
      wr_error (wh, ": reference to %#" PRIx64
		" points into another location or range list.\n", addr);
      retval = false;
    }

  uint64_t escape = cu->address_size == 8
    ? (uint64_t)-1 : (uint64_t)(uint32_t)-1;

  bool overlap = false;
  uint64_t base = cu->low_pc;
  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec->id, wh);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));

#define HAVE_OVERLAP						\
      do {							\
	wr_error (&where, ": range definitions overlap.\n");	\
	retval = false;						\
	overlap = true;						\
      } while (0)

      /* begin address */
      uint64_t begin_addr;
      uint64_t begin_off = read_ctx_get_offset (&ctx);
      GElf_Sym begin_symbol_mem, *begin_symbol = &begin_symbol_mem;
      bool begin_relocated = false;
      if (!overlap
	  && coverage_is_overlap (coverage, begin_off, cu->address_size))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, cu->address_size == 8, &begin_addr))
	{
	  wr_error (&where, ": can't read address range beginning.\n");
	  return false;
	}

      struct relocation *rel;
      if ((rel = relocation_next (&sec->rel, begin_off,
				  &where, skip_mismatched)))
	{
	  begin_relocated = true;
	  relocate_one (file, &sec->rel, rel, cu->address_size,
			&begin_addr, &where, rel_value,	&begin_symbol);
	}

      /* end address */
      uint64_t end_addr;
      uint64_t end_off = read_ctx_get_offset (&ctx);
      GElf_Sym end_symbol_mem, *end_symbol = &end_symbol_mem;
      bool end_relocated = false;
      if (!overlap
	  && coverage_is_overlap (coverage, end_off, cu->address_size))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, cu->address_size == 8, &end_addr))
	{
	  wr_error (&where, ": can't read address range ending.\n");
	  return false;
	}

      if ((rel = relocation_next (&sec->rel, end_off,
				  &where, skip_mismatched)))
	{
	  end_relocated = true;
	  relocate_one (file, &sec->rel, rel, cu->address_size,
			&end_addr, &where, rel_value, &end_symbol);
	  if (begin_addr != escape)
	    {
	      if (!begin_relocated)
		wr_message (cat | mc_impact_2 | mc_reloc, &where,
			    ": end of address range is relocated, but the beginning wasn't.\n");
	      else
		check_range_relocations (cat, &where, file,
					 begin_symbol, end_symbol,
					 "begin and end address");
	    }
	}
      else if (begin_relocated)
	wr_message (cat | mc_impact_2 | mc_reloc, &where,
		    ": end of address range is not relocated, but the beginning was.\n");

      bool done = false;
      if (begin_addr == 0 && end_addr == 0 && !begin_relocated && !end_relocated)
	done = true;
      else if (begin_addr != escape)
	{
	  if (base == (uint64_t)-1)
	    {
	      wr_error (&where,
			": address range with no base address set: %s.\n",
			range_fmt (buf, sizeof buf, begin_addr, end_addr));
	      /* This is not something that would derail high-level,
		 so carry on.  */
	    }

	  if (end_addr < begin_addr)
	    wr_message (cat | mc_error, &where,	": has negative range %s.\n",
			range_fmt (buf, sizeof buf, begin_addr, end_addr));
	  else if (begin_addr == end_addr)
	    /* 2.6.6: A location list entry [...] whose beginning
	       and ending addresses are equal has no effect.  */
	    wr_message (cat | mc_acc_bloat | mc_impact_3, &where,
			": entry covers no range.\n");
	  /* Skip coverage analysis if we have errors or have no base
	     (or just don't do coverage analysis at all).  */
	  else if (base < (uint64_t)-2 && retval
		   && (coverage_map != NULL || cu_coverage != NULL))
	    {
	      uint64_t address = begin_addr + base;
	      uint64_t length = end_addr - begin_addr;
	      if (coverage_map != NULL)
		coverage_map_add (coverage_map, address, length, &where, cat);
	      if (cu_coverage != NULL)
		coverage_add (&cu_coverage->cov, address, length);
	    }

	  if (contains_locations)
	    {
	      /* location expression length */
	      uint16_t len;
	      if (!overlap
		  && coverage_is_overlap (coverage,
					  read_ctx_get_offset (&ctx), 2))
		HAVE_OVERLAP;

	      if (!read_ctx_read_2ubyte (&ctx, &len))
		{
		  wr_error (&where, ": can't read length of location expression.\n");
		  return false;
		}

	      /* location expression itself */
	      uint64_t expr_start = read_ctx_get_offset (&ctx);
	      if (!check_location_expression (file, &ctx, cu, expr_start,
					      &sec->rel, len, &where))
		return false;
	      uint64_t expr_end = read_ctx_get_offset (&ctx);
	      if (!overlap
		  && coverage_is_overlap (coverage,
					  expr_start, expr_end - expr_start))
		HAVE_OVERLAP;

	      if (!read_ctx_skip (&ctx, len))
		{
		  /* "can't happen" */
		  wr_error (&where, PRI_NOT_ENOUGH, "location expression");
		  return false;
		}
	    }
	}
      else
	{
	  if (end_addr == base)
	    wr_message (cat | mc_acc_bloat | mc_impact_3, &where,
			": base address selection doesn't change base address"
			" (%#" PRIx64 ").\n", base);
	  else
	    base = end_addr;
	}
#undef HAVE_OVERLAP

      coverage_add (coverage, where.addr1, read_ctx_get_offset (&ctx) - where.addr1);
      if (done)
	break;
    }

  return retval;
}

bool
check_loc_or_range_structural (struct elf_file *file,
			       struct sec *sec,
			       struct cu *cu_chain,
			       struct cu_coverage *cu_coverage)
{
  assert (sec->id == sec_loc || sec->id == sec_ranges);
  assert (cu_chain != NULL);

  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  bool retval = true;

  /* For .debug_ranges, we optionally do ranges vs. ELF sections
     coverage analysis.  */
  struct coverage_map *coverage_map = NULL;
  if (do_range_coverage && sec->id == sec_ranges
      && (coverage_map
	    = coverage_map_alloc_XA (file, sec->id == sec_loc)) == NULL)
    {
      wr_error (&WHERE (sec->id, NULL),
		": couldn't read ELF, skipping coverage analysis.\n");
      retval = false;
    }

  /* Overlap discovery.  */
  struct coverage coverage;
  WIPE (coverage);

  enum message_category cat = sec->id == sec_loc ? mc_loc : mc_ranges;

  /* Relocation checking in the followings assumes that all the
     references are organized in monotonously increasing order.  That
     doesn't have to be the case.  So merge all the references into
     one sorted array.  */
  size_t size = 0;
  for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
    {
      struct ref_record *rec
	= sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
      size += rec->size;
    }
  struct ref_cu
  {
    struct ref ref;
    struct cu *cu;
  };
  struct ref_cu *refs = xmalloc (sizeof (*refs) * size);
  struct ref_cu *refptr = refs;
  for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
    {
      struct ref_record *rec
	= sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
      for (size_t i = 0; i < rec->size; ++i)
	*refptr++ = ((struct ref_cu){.ref = rec->refs[i], .cu = cu});
    }
  int compare_refs (const void *a, const void *b)
  {
    const struct ref_cu *ref_a = (const struct ref_cu *)a;
    const struct ref_cu *ref_b = (const struct ref_cu *)b;

    if (ref_a->ref.addr > ref_b->ref.addr)
      return 1;
    else if (ref_a->ref.addr < ref_b->ref.addr)
      return -1;
    else
      return 0;
  }
  qsort (refs, size, sizeof (*refs), compare_refs);

  uint64_t last_off = 0;
  for (size_t i = 0; i < size; ++i)
    {
      uint64_t off = refs[i].ref.addr;
      if (i > 0)
	{
	  if (off == last_off)
	    continue;
	  relocation_skip (&sec->rel, off,
			   &WHERE (sec->id, NULL), skip_unref);
	}

      /* XXX We pass cu_coverage down for all ranges.  That means all
	 ranges get recorded, not only those belonging to CUs.
	 Perhaps that's undesirable.  */
      if (!check_loc_or_range_ref (file, &ctx, refs[i].cu, sec,
				   &coverage, coverage_map,
				   sec->id == sec_ranges ? cu_coverage : NULL,
				   off, &refs[i].ref.who, cat))
	retval = false;
      last_off = off;
    }

  if (retval)
    {
      relocation_skip_rest (&sec->rel, sec->id);

      /* We check that all CUs have the same address size when building
	 the CU chain.  So just take the address size of the first CU in
	 chain.  */
      coverage_find_holes (&coverage, 0, ctx.data->d_size, found_hole,
			   &((struct hole_info)
			     {sec->id, cat, ctx.data->d_buf,
			      cu_chain->address_size}));

      if (coverage_map)
	coverage_map_find_holes (coverage_map, &coverage_map_found_hole,
				 &(struct coverage_map_hole_info)
				 {coverage_map->elf, {sec->id, cat, NULL, 0}});
    }

  coverage_free (&coverage);
  coverage_map_free_XA (coverage_map);

  if (retval && cu_coverage != NULL)
    /* Only drop the flag if we were successful, so that the coverage
       analysis isn't later done against incomplete data.  */
    cu_coverage->need_ranges = false;

  return retval;
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
