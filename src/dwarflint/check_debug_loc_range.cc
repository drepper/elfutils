/* Routines related to .debug_loc and .debug_range.

   Copyright (C) 2009 Red Hat, Inc.
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

// xxx drop as soon as not necessary
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <cassert>
#include <sstream>
#include "../libdw/dwarf.h"

#include "low.h"
#include "config.h"
#include "check_debug_loc_range.hh"

namespace
{
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

  struct coverage_map *
  coverage_map_alloc_XA (struct elf_file *elf, bool allow_overlap)
  {
    coverage_map *ret = (coverage_map *)xmalloc (sizeof (*ret));
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

  struct hole_env
  {
    struct where *where;
    uint64_t address;
    uint64_t end;
  };

  bool
  range_hole (uint64_t h_start, uint64_t h_length, void *xenv)
  {
    hole_env *env = (hole_env *)xenv;
    char buf[128], buf2[128];
    assert (h_length != 0);
    wr_error (env->where,
	      ": portion %s of the range %s "
	      "doesn't fall into any ALLOC section.\n",
	      range_fmt (buf, sizeof buf,
			 h_start + env->address, h_start + env->address + h_length),
	      range_fmt (buf2, sizeof buf2, env->address, env->end));
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

  struct wrap_cb_arg
  {
    bool (*cb) (uint64_t begin, uint64_t end,
		struct section_coverage *, void *);
    section_coverage *sco;
    void *user;
  };

  bool
  unwrap_cb (uint64_t h_start, uint64_t h_length, void *user)
  {
    wrap_cb_arg *arg = (wrap_cb_arg *)user;
    return (arg->cb) (h_start, h_start + h_length, arg->sco, arg->user);
  }

  bool
  coverage_map_find_holes (struct coverage_map *coverage_map,
			   bool (*cb) (uint64_t begin, uint64_t end,
				       struct section_coverage *, void *),
			   void *user)
  {
    for (size_t i = 0; i < coverage_map->size; ++i)
      {
	section_coverage *sco = coverage_map->scos + i;
	wrap_cb_arg arg = {cb, sco, user};
	if (!coverage_find_holes (&sco->cov, 0, sco->sec->shdr.sh_size,
				  unwrap_cb, &arg))
	  return false;
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
	hole_env env = {where, address, end};
	coverage_find_holes (&range_cov, 0, length, range_hole, &env);
      }

    coverage_free (&range_cov);
  }

  void
  coverage_map_free (struct coverage_map *coverage_map)
  {
    for (size_t i = 0; i < coverage_map->size; ++i)
      coverage_free (&coverage_map->scos[i].cov);
    free (coverage_map->scos);
  }

  void
  coverage_map_free_XA (coverage_map *coverage_map)
  {
    if (coverage_map != NULL)
      {
	coverage_map_free (coverage_map);
	free (coverage_map);
      }
  }

  bool
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

    uint64_t escape = cu->head->address_size == 8
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
	  retval = false;					\
	  overlap = true;					\
	} while (0)

	/* begin address */
	uint64_t begin_addr;
	uint64_t begin_off = read_ctx_get_offset (&ctx);
	GElf_Sym begin_symbol_mem, *begin_symbol = &begin_symbol_mem;
	bool begin_relocated = false;
	if (!overlap
	    && coverage_is_overlap (coverage, begin_off, cu->head->address_size))
	  HAVE_OVERLAP;

	if (!read_ctx_read_offset (&ctx, cu->head->address_size == 8, &begin_addr))
	  {
	    wr_error (&where, ": can't read address range beginning.\n");
	    return false;
	  }

	struct relocation *rel;
	if ((rel = relocation_next (&sec->rel, begin_off,
				    &where, skip_mismatched)))
	  {
	    begin_relocated = true;
	    relocate_one (file, &sec->rel, rel, cu->head->address_size,
			  &begin_addr, &where, rel_value,	&begin_symbol);
	  }

	/* end address */
	uint64_t end_addr;
	uint64_t end_off = read_ctx_get_offset (&ctx);
	GElf_Sym end_symbol_mem, *end_symbol = &end_symbol_mem;
	bool end_relocated = false;
	if (!overlap
	    && coverage_is_overlap (coverage, end_off, cu->head->address_size))
	  HAVE_OVERLAP;

	if (!read_ctx_read_offset (&ctx, cu->head->address_size == 8, &end_addr))
	  {
	    wr_error (&where, ": can't read address range ending.\n");
	    return false;
	  }

	if ((rel = relocation_next (&sec->rel, end_off,
				    &where, skip_mismatched)))
	  {
	    end_relocated = true;
	    relocate_one (file, &sec->rel, rel, cu->head->address_size,
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

  struct ref_cu
  {
    struct ref ref;
    struct cu *cu;
  };

  int
  compare_refs (const void *a, const void *b)
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
	wr_error (WHERE (sec->id, NULL))
	  << "couldn't read ELF, skipping coverage analysis." << std::endl;
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
    struct ref_cu *refs = (ref_cu *)xmalloc (sizeof (*refs) * size);
    struct ref_cu *refptr = refs;
    for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
      {
	struct ref_record *rec
	  = sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
	for (size_t i = 0; i < rec->size; ++i)
	  *refptr++ = {rec->refs[i], cu};
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
	    struct where wh = WHERE (sec->id, NULL);
	    relocation_skip (&sec->rel, off, &wh, skip_unref);
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
	struct hole_info hi = {
	  sec->id, cat, ctx.data->d_buf, cu_chain->head->address_size
	};
	coverage_find_holes (&coverage, 0, ctx.data->d_size, found_hole, &hi);

	if (coverage_map)
	  {
	    struct coverage_map_hole_info cmhi = {
	      coverage_map->elf, {sec->id, cat, NULL, 0}
	    };
	    coverage_map_find_holes (coverage_map, &coverage_map_found_hole,
				     &cmhi);
	  }
      }

    coverage_free (&coverage);
    coverage_map_free_XA (coverage_map);

    if (retval && cu_coverage != NULL)
      /* Only drop the flag if we were successful, so that the coverage
	 analysis isn't later done against incomplete data.  */
      cu_coverage->need_ranges = false;

    return retval;
  }
}

check_debug_ranges::check_debug_ranges (dwarflint &lint)
  : _m_sec_ranges (lint.check (_m_sec_ranges))
  , _m_cus (lint.check (_m_cus))
{
  if (!::check_loc_or_range_structural (&_m_sec_ranges->file,
					&_m_sec_ranges->sect,
					&_m_cus->cus.front (),
					&_m_cus->cu_cov))
    throw check_base::failed ();
}

check_debug_loc::check_debug_loc (dwarflint &lint)
  : _m_sec_loc (lint.check (_m_sec_loc))
  , _m_cus (lint.check (_m_cus))
{
  if (!::check_loc_or_range_structural (&_m_sec_loc->file,
					&_m_sec_loc->sect,
					&_m_cus->cus.front (),
					NULL))
    throw check_base::failed ();
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
	{
	  struct where wh = WHERE (info->section, NULL);
	  wr_message_padding_0 (info->category, &wh, start, end);
	}
    }
  else
    {
      /* XXX: This actually lies when the unreferenced portion is
	 composed of sequences of zeroes and non-zeroes.  */
      struct where wh = WHERE (info->section, NULL);
      wr_message_padding_n0 (info->category, &wh, start, end);
    }

  return true;
}

namespace
{
  reg<check_debug_ranges> reg_debug_ranges;
  reg<check_debug_loc> reg_debug_loc;
}
