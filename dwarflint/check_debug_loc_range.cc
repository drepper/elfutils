/* Routines related to .debug_loc and .debug_range.

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

// xxx drop as soon as not necessary
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <cassert>
#include <sstream>
#include <algorithm>

#include "../libdw/c++/dwarf"
#include "../src/dwarf-opcodes.h"

#include "elf_file.hh"
#include "check_debug_loc_range.hh"
#include "check_debug_info.hh"
#include "sections.hh"
#include "checked_read.hh"
#include "pri.hh"
#include "misc.hh"

bool do_range_coverage = false; // currently no option

global_opt<void_option>
  opt_show_refs("\
When validating .debug_loc and .debug_ranges, display information about \
the DIE referring to the entry in consideration", "ref");

std::string
loc_range_locus::format (bool brief) const
{
  std::stringstream ss;
  if (!brief)
    ss << section_name[_m_sec] << ": ";

  if (_m_sec == sec_loc)
    ss << "loclist";
  else
    ss << "rangelist";

  if (_m_offset != (Dwarf_Off)-1)
    ss << " 0x" << std::hex << _m_offset;

  if (opt_show_refs)
    ss << ", ref. by " << _m_refby.format (true);

  return ss.str ();
}

checkdescriptor const *
check_debug_ranges::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_ranges")
     .groups ("@low")
     .schedule (false)
     .description (
"Checks for low-level structure of .debug_ranges.  In addition it "
"checks:\n"
" - for overlapping and dangling references from .debug_info\n"
" - that base address is set and that it actually changes the address\n"
" - that ranges have a positive size\n"
" - that there are no unreferenced holes in the section\n"
" - that relocations are valid.  In ET_REL files that certain fields "
"are relocated\n"
" - neither or both of range start and range end are expected to be "
"relocated.  It's expected that they are both relocated against the "
"same section.\n"));
  return &cd;
}

static reg<check_debug_ranges> reg_debug_ranges;

checkdescriptor const *
check_debug_loc::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_loc")
     .groups ("@low")
     .schedule (false)
     .description (
"Checks for low-level structure of .debug_loc.  In addition it "
"makes the same checks as .debug_ranges.  For location expressions "
"it further checks:\n"
" - that DW_OP_bra and DW_OP_skip argument is non-zero and doesn't "
"escape the expression.  In addition it is required that the jump "
"ends on another instruction, not arbitrarily in the middle of the "
"byte stream, even if that position happened to be interpretable as "
"another well-defined instruction stream.\n"
" - on 32-bit machines it rejects DW_OP_const8u and DW_OP_const8s\n"
" - on 32-bit machines it checks that ULEB128-encoded arguments aren't "
"quantities that don't fit into 32 bits\n"));
  return &cd;
}

static reg<check_debug_loc> reg_debug_loc;

namespace
{
  bool
  coverage_map_init (struct coverage_map *coverage_map,
		     struct elf_file *elf,
		     Elf64_Xword mask,
		     Elf64_Xword warn_mask,
		     bool allow_overlap)
  {
    assert (coverage_map != NULL);
    assert (elf != NULL);

    coverage_map->elf = elf;
    coverage_map->allow_overlap = allow_overlap;

    for (size_t i = 1; i < elf->size; ++i)
      {
	struct sec *sec = elf->sec + i;

	bool normal = (sec->shdr.sh_flags & mask) == mask;
	bool warn = (sec->shdr.sh_flags & warn_mask) == warn_mask;
	if (normal || warn)
	  coverage_map->scos
	    .push_back (section_coverage (sec, !normal));
      }

    return true;
  }

  struct coverage_map *
  coverage_map_alloc_XA (struct elf_file *elf, bool allow_overlap)
  {
    coverage_map *ret = new coverage_map ();
    if (!coverage_map_init (ret, elf,
			    SHF_EXECINSTR | SHF_ALLOC,
			    SHF_ALLOC,
			    allow_overlap))
      {
	delete ret;
	return NULL;
      }
    return ret;
  }

  struct hole_env
  {
    locus const &loc;
    uint64_t address;
    uint64_t end;
  };

  bool
  range_hole (uint64_t h_start, uint64_t h_length, void *xenv)
  {
    hole_env *env = (hole_env *)xenv;
    char buf[128], buf2[128];
    assert (h_length != 0);
    wr_error (&env->loc,
	      ": portion %s of the range %s "
	      "doesn't fall into any ALLOC section.\n",
	      range_fmt (buf, sizeof buf,
			 h_start + env->address, h_start + env->address + h_length),
	      range_fmt (buf2, sizeof buf2, env->address, env->end));
    return true;
  }

  struct coverage_map_hole_info
  {
    struct elf_file *elf;
    struct hole_info info;
  };

  /* begin is inclusive, end is exclusive. */
  bool
  coverage_map_found_hole (uint64_t begin, uint64_t end,
			   struct section_coverage *sco, void *user)
  {
    struct coverage_map_hole_info *info = (struct coverage_map_hole_info *)user;

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
    wr_message (section_locus (info->info.section),
		info->info.category | mc_acc_suboptimal | mc_impact_4)
      << "addresses " << range_fmt (buf, sizeof buf, begin + base, end + base)
      << " of section " << scnname << " are not covered.\n";
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
	section_coverage *sco = &coverage_map->scos[i];
	wrap_cb_arg arg = {cb, sco, user};
	if (!sco->cov.find_holes (0, sco->sec->shdr.sh_size, unwrap_cb, &arg))
	  return false;
      }

    return true;
  }

  void
  coverage_map_add (struct coverage_map *coverage_map,
		    uint64_t address,
		    uint64_t length,
		    locus const &loc,
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
    coverage range_cov;

    for (size_t i = 0; i < coverage_map->size; ++i)
      {
	struct section_coverage *sco = &coverage_map->scos[i];
	GElf_Shdr *shdr = &sco->sec->shdr;
	struct coverage *cov = &sco->cov;

	Elf64_Addr s_end = shdr->sh_addr + shdr->sh_size;
	if (end <= shdr->sh_addr || address >= s_end)
	  /* no overlap */
	  continue;

	if (found && !crosses_boundary)
	  {
	    /* While probably not an error, it's very suspicious.  */
	    wr_message (cat | mc_impact_2, &loc,
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
	    && cov->is_overlap (cov_begin, cov_end - cov_begin))
	  {
	    /* Not a show stopper, this shouldn't derail high-level.  */
	    wr_message (loc, cat | mc_aranges | mc_impact_2 | mc_error)
	      << "the range " << range_fmt (buf, sizeof buf, address, end)
	      << " overlaps with another one." << std::endl;
	    overlap = true;
	  }

	if (sco->warn)
	  wr_message (cat | mc_impact_2, &loc,
		      ": the range %s covers section %s.\n",
		      range_fmt (buf, sizeof buf, address, end), sco->sec->name);

	/* Section coverage... */
	cov->add (cov_begin, cov_end - cov_begin);
	sco->hit = true;

	/* And range coverage... */
	range_cov.add (r_cov_begin, r_cov_end - r_cov_begin);
      }

    if (!found)
      /* Not a show stopper.  */
      wr_error (&loc,
		": couldn't find a section that the range %s covers.\n",
		range_fmt (buf, sizeof buf, address, end));
    else if (length > 0)
      {
	hole_env env = {loc, address, end};
	range_cov.find_holes (0, length, range_hole, &env);
      }
  }

  bool
  check_loc_or_range_ref (dwarf_version const *ver,
			  struct elf_file *file,
			  const struct read_ctx *parent_ctx,
			  struct cu *cu,
			  struct sec *sec,
			  struct coverage *coverage,
			  struct coverage_map *coverage_map,
			  struct coverage *pc_coverage,
			  uint64_t addr,
			  locus const &loc,
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
	wr_error (&loc, ": invalid reference outside the section "
		  "%#" PRIx64 ", size only %#tx.\n",
		  addr, ctx.end - ctx.begin);
	return false;
      }

    bool retval = true;
    bool contains_locations = sec->id == sec_loc;

    if (coverage->is_covered (addr, 1))
      {
	wr_error (&loc, ": reference to %#" PRIx64
		  " points into another location or range list.\n", addr);
	retval = false;
      }

    uint64_t escape = cu->head->address_size == 8
      ? (uint64_t)-1 : (uint64_t)(uint32_t)-1;

    bool overlap = false;
    uint64_t base = cu->low_pc;
    while (!read_ctx_eof (&ctx))
      {
	uint64_t offset = read_ctx_get_offset (&ctx);
	loc_range_locus where (sec->id, loc, offset);

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
	    && coverage->is_overlap (begin_off, cu->head->address_size))
	  HAVE_OVERLAP;

	if (!read_ctx_read_offset (&ctx, cu->head->address_size == 8, &begin_addr))
	  {
	    wr_error (&where, ": can't read address range beginning.\n");
	    return false;
	  }

	struct relocation *rel;
	if ((rel = relocation_next (&sec->rel, begin_off,
				    where, skip_mismatched)))
	  {
	    begin_relocated = true;
	    relocate_one (file, &sec->rel, rel, cu->head->address_size,
			  &begin_addr, where, rel_target::rel_value,
			  &begin_symbol);
	  }

	/* end address */
	uint64_t end_addr;
	uint64_t end_off = read_ctx_get_offset (&ctx);
	GElf_Sym end_symbol_mem, *end_symbol = &end_symbol_mem;
	bool end_relocated = false;
	if (!overlap
	    && coverage->is_overlap (end_off, cu->head->address_size))
	  HAVE_OVERLAP;

	if (!read_ctx_read_offset (&ctx, cu->head->address_size == 8,
				   &end_addr))
	  {
	    wr_error (&where, ": can't read address range ending.\n");
	    return false;
	  }

	if ((rel = relocation_next (&sec->rel, end_off,
				    where, skip_mismatched)))
	  {
	    end_relocated = true;
	    relocate_one (file, &sec->rel, rel, cu->head->address_size,
			  &end_addr, where, rel_target::rel_value, &end_symbol);
	    if (begin_addr != escape)
	      {
		if (!begin_relocated)
		  wr_message (cat | mc_impact_2 | mc_reloc, &where,
			      ": end of address range is relocated, but the beginning wasn't.\n");
		else
		  check_range_relocations (where, cat, file,
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
	      wr_message (cat | mc_error, &where, ": has negative range %s.\n",
			  range_fmt (buf, sizeof buf, begin_addr, end_addr));
	    else if (begin_addr == end_addr)
	      /* 2.6.6: A location list entry [...] whose beginning
		 and ending addresses are equal has no effect.  */
	      wr_message (cat | mc_acc_bloat | mc_impact_3, &where,
			  ": entry covers no range.\n");
	    /* Skip coverage analysis if we have errors or have no base
	       (or just don't do coverage analysis at all).  */
	    else if (base < (uint64_t)-2 && retval
		     && (coverage_map != NULL || pc_coverage != NULL))
	      {
		uint64_t address = begin_addr + base;
		uint64_t length = end_addr - begin_addr;
		if (coverage_map != NULL)
		  coverage_map_add (coverage_map, address, length, where, cat);
		if (pc_coverage != NULL)
		  pc_coverage->add (address, length);
	      }

	    if (contains_locations)
	      {
		/* location expression length */
		uint16_t len;
		if (!overlap
		    && coverage->is_overlap (read_ctx_get_offset (&ctx), 2))
		  HAVE_OVERLAP;

		if (!read_ctx_read_2ubyte (&ctx, &len))
		  {
		    wr_error (where)
		      << "can't read length of location expression."
		      << std::endl;
		    return false;
		  }

		/* location expression itself */
		uint64_t expr_start = read_ctx_get_offset (&ctx);
		if (!check_location_expression
		    (ver, *file, &ctx, cu, expr_start, &sec->rel, len, where))
		  return false;
		uint64_t expr_end = read_ctx_get_offset (&ctx);
		if (!overlap
		    && coverage->is_overlap (expr_start, expr_end - expr_start))
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

	coverage->add (offset, read_ctx_get_offset (&ctx) - offset);
	if (done)
	  break;
      }

    return retval;
  }

  struct ref_cu
  {
    ::ref ref;
    ::cu *cu;

    bool
    operator < (ref_cu const& other) const
    {
      return ref.addr < other.ref.addr;
    }
  };

  bool
  check_loc_or_range_structural (struct elf_file *file,
				 struct sec *sec,
				 struct cu *cu_chain,
				 struct coverage *pc_coverage)
  {
    assert (sec->id == sec_loc || sec->id == sec_ranges);
    assert (cu_chain != NULL);

    struct read_ctx ctx;
    read_ctx_init (&ctx, sec->data, file->other_byte_order);

    bool retval = true;

    /* For .debug_ranges, we optionally do ranges vs. ELF sections
       coverage analysis.  */
    // xxx this is a candidate for a separate check
    struct coverage_map *coverage_map = NULL;
    if (do_range_coverage && sec->id == sec_ranges
	&& (coverage_map
	    = coverage_map_alloc_XA (file, sec->id == sec_loc)) == NULL)
      {
	wr_error (section_locus (sec->id))
	  << "couldn't read ELF, skipping coverage analysis." << std::endl;
	retval = false;
      }

    /* Overlap discovery.  */
    struct coverage coverage;

    enum message_category cat = sec->id == sec_loc ? mc_loc : mc_ranges;

    {
    /* Relocation checking in the followings assumes that all the
       references are organized in monotonously increasing order.  That
       doesn't have to be the case.  So merge all the references into
       one sorted array.  */
    typedef std::vector<ref_cu> ref_cu_vect;
    ref_cu_vect refs;
    for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
      {
	ref_record *rec
	  = sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
	for (ref_record::const_iterator it = rec->begin ();
	     it != rec->end (); ++it)
	  {
	    ref_cu ref = {*it, cu};
	    refs.push_back (ref);
	  }
      }
    std::sort (refs.begin (), refs.end ());

    uint64_t last_off = 0;
    for (ref_cu_vect::const_iterator it = refs.begin ();
	 it != refs.end (); ++it)
      {
	uint64_t off = it->ref.addr;
	if (it != refs.begin ())
	  {
	    if (off == last_off)
	      continue;
	    relocation_skip (&sec->rel, off, section_locus (sec->id),
			     skip_unref);
	  }

	// xxx right now this is just so that we can ver->get_form
	// down the road, which is just a result of the way
	// dwarf-opcodes encode operator operand types.  But in the
	// future, we'd like versions to support also queries for
	// operators and their operands, so keep it.
	dwarf_version const *ver
	  = dwarf_version::get (it->cu->head->version);

	/* XXX We pass cu_coverage down for all ranges.  That means all
	   ranges get recorded, not only those belonging to CUs.
	   Perhaps that's undesirable.  */
	if (!check_loc_or_range_ref (ver, file, &ctx, it->cu, sec,
				     &coverage, coverage_map, pc_coverage,
				     off, it->ref.who, cat))
	  retval = false;
	last_off = off;
      }
    }

    if (retval)
      {
	relocation_skip_rest (&sec->rel, section_locus (sec->id));

	/* We check that all CUs have the same address size when building
	   the CU chain.  So just take the address size of the first CU in
	   chain.  */
	struct hole_info hi = {
	  sec->id, cat, ctx.data->d_buf, (unsigned)cu_chain->head->address_size
	};
	coverage.find_holes (0, ctx.data->d_size, found_hole, &hi);

	if (coverage_map)
	  {
	    struct coverage_map_hole_info cmhi = {
	      coverage_map->elf, {sec->id, cat, NULL, 0}
	    };
	    coverage_map_find_holes (coverage_map, &coverage_map_found_hole,
				     &cmhi);
	  }
      }

    delete coverage_map;

    return retval;
  }
}

section_coverage::section_coverage (struct sec *a_sec, bool a_warn)
  : sec (a_sec)
  , hit (false)
  , warn (a_warn)
{
  assert (a_sec);
}

check_debug_ranges::check_debug_ranges (checkstack &stack, dwarflint &lint)
  : _m_sec_ranges (lint.check (stack, _m_sec_ranges))
  , _m_info (lint.check (stack, _m_info))
{
  memset (&_m_cov, 0, sizeof (_m_cov));
  if (!::check_loc_or_range_structural (&_m_sec_ranges->file,
					&_m_sec_ranges->sect,
					&_m_info->cus.front (),
					&_m_cov))
    throw check_base::failed ();
}

check_debug_loc::check_debug_loc (checkstack &stack, dwarflint &lint)
  : _m_sec_loc (lint.check (stack, _m_sec_loc))
  , _m_info (lint.check (stack, _m_info))
{
  if (!::check_loc_or_range_structural (&_m_sec_loc->file,
					&_m_sec_loc->sect,
					&_m_info->cus.front (),
					NULL))
    throw check_base::failed ();
}

namespace
{
  /* Operands are passed back as attribute forms.  In particular,
     DW_FORM_dataX for X-byte operands, DW_FORM_[us]data for
     ULEB128/SLEB128 operands, and DW_FORM_addr/DW_FORM_ref_addr
     for 32b/64b operands.
     If the opcode takes no operands, 0 is passed.

     Return value is false if we couldn't determine (i.e. invalid
     opcode).
  */

  bool
  get_location_opcode_operands (dwarf_version const *ver,
				uint8_t opcode,
				form const **f1p,
				form const **f2p)
  {
    int op1, op2;
    switch (opcode)
      {
#define DW_OP_2(OPCODE, OP1, OP2)				\
	case OPCODE: op1 = OP1; op2 = OP2; break;
#define DW_OP_1(OPCODE, OP1) DW_OP_2(OPCODE, OP1, 0)
#define DW_OP_0(OPCODE) DW_OP_2(OPCODE, 0, 0)

	DW_OP_OPERANDS

#undef DEF_DW_OP_2
#undef DEF_DW_OP_1
#undef DEF_DW_OP_0
      default:
	return false;
      };

#define RETV(OP,P)		\
    if (OP != 0)		\
      {				\
	form const *f = NULL;	\
	f = ver->get_form (OP);	\
	if (f == NULL)		\
	  return false;		\
	*P = f;			\
      }				\
    else			\
      *P = NULL;

    RETV (op1, f1p);
    RETV (op2, f2p);
    return true;
  }

  static rel_target
  reloc_target_loc (uint8_t opcode)
  {
    switch (opcode)
      {
      case DW_OP_call2:
      case DW_OP_call4:
	return sec_info;

      case DW_OP_addr:
	return rel_target::rel_address;

      case DW_OP_call_ref:
	assert (!"Can't handle call_ref!");
      };

    std::cout << "XXX don't know how to handle opcode="
	      << elfutils::dwarf::ops::name (opcode) << std::endl;

    return rel_target::rel_value;
  }

  bool
  op_read_form (struct elf_file const &file,
		struct read_ctx *ctx,
		struct cu *cu,
		uint64_t init_off,
		struct relocation_data *reloc,
		int opcode,
		form const *form,
		uint64_t *valuep,
		char const *str,
		locus const &where)
  {
    if (form == NULL)
      return true;

    uint64_t off = read_ctx_get_offset (ctx) + init_off;

    storage_class_t storclass = form->storage_class ();
    assert (storclass != sc_string);
    if (!read_generic_value (ctx, form->width (cu->head), storclass,
			     where, valuep, NULL))
      {
	wr_error (where)
	  << "opcode \"" << elfutils::dwarf::ops::name (opcode)
	  << "\": can't read " << str << " (form \""
	  << *form << "\")." << std::endl;
	return false;
      }

    /* For non-block forms, allow relocation of the datum.  For block
       form, allow relocation of block contents, but not the
       block length).  */

    struct relocation *rel;
    if ((rel = relocation_next (reloc, off,
				where, skip_mismatched)))
      {
	if (storclass != sc_block)
	  relocate_one (&file, reloc, rel,
			cu->head->address_size, valuep, where,
			reloc_target_loc (opcode), NULL);
	else
	  wr_error (where) << "relocation relocates a length field.\n";
      }
    if (storclass == sc_block)
      {
	uint64_t off_block_end = read_ctx_get_offset (ctx) + init_off - 1;
	relocation_next (reloc, off_block_end, where, skip_ok);
      }

    return true;
  }
}

class locexpr_locus
  : public locus
{
  uint64_t _m_offset;
  locus const *_m_context;

public:
  explicit locexpr_locus (uint64_t offset, locus const *context)
    : _m_offset (offset)
    , _m_context (context)
  {}

  std::string
  format (bool) const
  {
    std::stringstream ss;
    ss << _m_context
       << " (location expression offset 0x" << std::hex << _m_offset << ")";
    return ss.str ();
  }
};

bool
check_location_expression (dwarf_version const *ver,
			   elf_file const &file,
			   struct read_ctx *parent_ctx,
			   struct cu *cu,
			   uint64_t init_off,
			   struct relocation_data *reloc,
			   size_t length,
			   locus const &loc)
{
  struct read_ctx ctx;
  if (!read_ctx_init_sub (&ctx, parent_ctx, parent_ctx->ptr,
			  parent_ctx->ptr + length))
    {
      wr_error (&loc, PRI_NOT_ENOUGH, "location expression");
      return false;
    }

  typedef ref_T<locexpr_locus> locexpr_ref;
  typedef ref_record_T<locexpr_locus> locexpr_ref_record;
  locexpr_ref_record oprefs;
  addr_record opaddrs;

  while (!read_ctx_eof (&ctx))
    {
      uint64_t opcode_off = read_ctx_get_offset (&ctx) + init_off;
      locexpr_locus where (opcode_off, &loc);
      opaddrs.add (opcode_off);

      uint8_t opcode;
      if (!read_ctx_read_ubyte (&ctx, &opcode))
	{
	  wr_error (&where, ": can't read opcode.\n");
	  break;
	}

      form const *form1 = NULL;
      form const *form2 = NULL;
      if (!get_location_opcode_operands (ver, opcode, &form1, &form2))
	{
	  wr_error (where)
	    << "can't decode opcode \""
	    << elfutils::dwarf::ops::name (opcode) << "\"." << std::endl;
	  break;
	}

      uint64_t value1, value2;
      if (!op_read_form (file, &ctx, cu, init_off, reloc,
			 opcode, form1, &value1, "1st operand", where)
	  || !op_read_form (file, &ctx, cu, init_off, reloc,
			    opcode, form2, &value2, "2st operand", where))
	goto out;

      switch (opcode)
	{
	case DW_OP_bra:
	case DW_OP_skip:
	  {
	    int16_t skip = (uint16_t)value1;

	    if (skip == 0)
	      wr_message (where, mc_loc | mc_acc_bloat | mc_impact_3)
		<< elfutils::dwarf::ops::name (opcode)
		<< " with skip 0." << std::endl;
	    else if (skip > 0 && !read_ctx_need_data (&ctx, (size_t)skip))
	      wr_error (where)
		<< elfutils::dwarf::ops::name (opcode)
		<< " branches out of location expression." << std::endl;
	    /* Compare with the offset after the two-byte skip value.  */
	    else if (skip < 0 && ((uint64_t)-skip) > read_ctx_get_offset (&ctx))
	      wr_error (where)
		<< elfutils::dwarf::ops::name (opcode)
		<< " branches before the beginning of location expression."
		<< std::endl;
	    else
	      {
		uint64_t off_after = read_ctx_get_offset (&ctx) + init_off;
		oprefs.push_back (locexpr_ref (off_after + skip, where));
	      }

	    break;
	  }

	case DW_OP_const8u:
	case DW_OP_const8s:
	  if (cu->head->address_size == 4)
	    wr_error (where)
	      << elfutils::dwarf::ops::name (opcode) << " on 32-bit machine."
	      << std::endl;
	  break;

	default:
	  if (cu->head->address_size == 4
	      && (opcode == DW_OP_constu
		  || opcode == DW_OP_consts
		  || opcode == DW_OP_deref_size
		  || opcode == DW_OP_plus_uconst)
	      && (value1 > (uint64_t)(uint32_t)-1))
	    wr_message (where, mc_loc | mc_acc_bloat | mc_impact_3)
	      << elfutils::dwarf::ops::name (opcode)
	      << " with operand " << pri::hex (value1)
	      << " on a 32-bit machine." << std::endl;
	}
    }

 out:
  for (locexpr_ref_record::const_iterator it = oprefs.begin ();
       it != oprefs.end (); ++it)
    if (!opaddrs.has_addr (it->addr))
      wr_error (it->who) << "unresolved reference to opcode at "
			 << pri::hex (it->addr) << ".\n";

  return true;
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
      if (info->align == 0 || info->align == 1
	  || length > info->align     // excessive
	  || end % info->align != 0   // doesn't actually align
	  || start % info->align == 0)// was already aligned
	wr_message_padding_0 (info->category, section_locus (info->section),
			      start, end);
    }
  else
    /* XXX: This actually lies when the unreferenced portion is
       composed of sequences of zeroes and non-zeroes.  */
    wr_message_padding_n0 (info->category, section_locus (info->section),
			   start, end);

  return true;
}

void
check_range_relocations (locus const &loc,
			 enum message_category cat,
			 struct elf_file const *file,
			 GElf_Sym *begin_symbol,
			 GElf_Sym *end_symbol,
			 const char *description)
{
  if (begin_symbol != NULL
      && end_symbol != NULL
      && begin_symbol->st_shndx != end_symbol->st_shndx)
    wr_message (cat | mc_impact_2 | mc_reloc, &loc,
		": %s relocated against different sections (%s and %s).\n",
		description,
		file->sec[begin_symbol->st_shndx].name,
		file->sec[end_symbol->st_shndx].name);
}
