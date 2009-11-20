/* Routines related to .debug_info.

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
#define PRI_DIE "DIE 0x%" PRIx64
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <cassert>

#include "messages.h"
#include "low.h"
#include "checks-low.hh"
#include "pri.hh"
#include "config.h"

namespace
{
  bool
  check_category (enum message_category cat)
  {
    return message_accept (&warning_criteria, cat);
  }

  bool
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

  bool
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

  std::vector <cu_head>
  read_info_headers (struct elf_file *file,
		     struct sec *sec,
		     struct relocation_data *reloc)
  {
    struct read_ctx ctx;
    read_ctx_init (&ctx, sec->data, file->other_byte_order);

    std::vector <cu_head> ret;
    while (!read_ctx_eof (&ctx))
      {
	const unsigned char *cu_begin = ctx.ptr;
	struct where where = WHERE (sec_info, NULL);
	where_reset_1 (&where, read_ctx_get_offset (&ctx));

	cu_head head;
	head.offset = where.addr1;
	head.where = where;

	/* Reading CU head is a bit tricky, because we don't know if
	   we have run into (superfluous but allowed) zero padding
	   between CUs.  */
	if (!read_ctx_need_data (&ctx, 4)
	    && check_zero_padding (&ctx, cat (mc_info, mc_header), &where))
	  break;

	/* CU length.  In DWARF 2, (uint32_t)-1 is simply a CU of that
	   length.  In DWARF 3+ that's an escape for 64bit length.
	   Unfortunately to read CU version, we have to get through
	   this field.  So we just assume that (uint32_t)-1 is an
	   escape in all cases.  */
	uint32_t size32;
	if (!read_ctx_read_4ubyte (&ctx, &size32))
	  {
	    wr_error (where) << "can't read CU length." << std::endl;
	    throw check_base::failed ();
	  }
	if (size32 == 0
	    && check_zero_padding (&ctx, cat (mc_info, mc_header), &where))
	  break;

	Dwarf_Off cu_size;
	if (!read_size_extra (&ctx, size32, &cu_size,
			      &head.offset_size, &where))
	  throw check_base::failed ();

	if (!read_ctx_need_data (&ctx, cu_size))
	  {
	    wr_error (where)
	      << "section doesn't have enough data to read CU of size "
	      << cu_size << '.' << std::endl;
	    throw check_base::failed ();
	  }

	/* CU size captures the size from the end of the length field
	   to the end of the CU.  */
	const unsigned char *cu_end = ctx.ptr + cu_size;

    	/* Version.  */
	uint16_t version;
	if (!read_ctx_read_2ubyte (&ctx, &version))
	  {
	    wr_error (head.where) << "can't read version." << std::endl;
	    throw check_base::failed ();
	  }
	if (get_dwarf_version (version) == NULL)
	  {
	    wr_error (head.where) << "unsupported CU version "
				  << version << '.' << std::endl;
	    throw check_base::failed ();
	  }
	if (version == 2 && head.offset_size == 8) // xxx?
	  /* Keep going.  It's a standard violation, but we may still
    	     be able to read the unit under consideration and do
    	     high-level checks.  */
	  wr_error (head.where) << "invalid 64-bit unit in DWARF 2 format.\n";
	head.version = version;

	/* Abbrev offset.  */
	uint64_t ctx_offset = read_ctx_get_offset (&ctx) + head.offset;
	if (!read_ctx_read_offset (&ctx, head.offset_size == 8,
				   &head.abbrev_offset))
	  {
	    wr_error (head.where) << "can't read abbrev offset." << std::endl;
	    throw check_base::failed ();
	  }

	struct relocation *rel
	  = relocation_next (reloc, ctx_offset, &head.where, skip_ok);
	if (rel != NULL)
	  {
	    relocate_one (file, reloc, rel, head.offset_size,
			  &head.abbrev_offset, &head.where, sec_abbrev, NULL);
	    rel->invalid = true; // mark as invalid so it's skipped
    				 // next time we pass by this
	  }
	else if (file->ehdr.e_type == ET_REL)
	  wr_message (head.where, cat (mc_impact_2, mc_info, mc_reloc))
	    << pri::lacks_relocation ("abbrev offset") << std::endl;

	/* Address size.  */
	if (!read_address_size (file, &ctx, &head.address_size, &head.where))
	  throw check_base::failed ();

	head.head_size = ctx.ptr - cu_begin; // Length of the headers itself.
	head.total_size = cu_end - cu_begin; // Length including headers field.
	head.size = head.total_size - head.head_size;

	if (!read_ctx_skip (&ctx, head.size))
	  {
	    wr_error (where) << pri::not_enough ("next CU") << std::endl;
	    throw check_base::failed ();
	  }

	ret.push_back (head);
      }

    return ret;
  }

  bool
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
      fprintf (stderr, "%s: CU starts\n", where_fmt (&cu->head->where, NULL));
    bool retval = true;

    dwarf_version_h ver = get_dwarf_version (cu->head->version);
    assert (ver != NULL);

    /* Look up Abbrev table for this CU.  */
    struct abbrev_table *abbrevs = abbrev_chain;
    for (; abbrevs != NULL; abbrevs = abbrevs->next)
      if (abbrevs->offset == cu->head->abbrev_offset)
	break;

    if (abbrevs == NULL)
      {
	wr_error (&cu->head->where,
		  ": couldn't find abbrev section with offset %" PRId64 ".\n",
		  cu->head->abbrev_offset);
	return false;
      }

    /* Read DIEs.  */
    struct ref_record local_die_refs;
    WIPE (local_die_refs);

    cu->cudie_offset = read_ctx_get_offset (ctx) + cu->head->offset;
    if (read_die_chain (ver, file, ctx, cu, abbrevs, strings,
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
			 struct cu_coverage *cu_coverage,
			 std::vector <cu_head> const &cu_headers)
  {
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
    if (reloc != NULL)
      relocation_reset (reloc);

    struct read_ctx ctx;
    read_ctx_init (&ctx, sec->data, file->other_byte_order);
    for (std::vector <cu_head>::const_iterator it = cu_headers.begin ();
	 it != cu_headers.end (); ++it)
      {
	cu_head const &head = *it;
	where const &where = head.where;
	struct cu *cur = (cu *)xcalloc (1, sizeof (*cur));
	cur->head = &head;
	cur->low_pc = (uint64_t)-1;
	cur->next = cu_chain;
	cu_chain = cur;

	assert (read_ctx_need_data (&ctx, head.total_size));

	// Make CU context begin just before the CU length, so that
	// DIE offsets are computed correctly.
	struct read_ctx cu_ctx;
	const unsigned char *cu_end = ctx.ptr + head.total_size;
	read_ctx_init_sub (&cu_ctx, &ctx, ctx.ptr, cu_end);
	cu_ctx.ptr += head.head_size;

	if (!check_cu_structural (file, &cu_ctx, cur, abbrev_chain,
				  strings, strings_coverage, reloc,
				  cu_coverage))
	  {
	    success = false;
	    break;
	  }

	if (cu_ctx.ptr != cu_ctx.end
	    && !check_zero_padding (&cu_ctx, mc_info, &where))
	  {
	    // Garbage coordinates:
	    uint64_t start
	      = read_ctx_get_offset (&ctx) + read_ctx_get_offset (&cu_ctx);
	    uint64_t end = read_ctx_get_offset (&ctx) + head.total_size;
	    wr_message_padding_n0 (mc_info, &where, start, end);
	  }

	int i = read_ctx_skip (&ctx, head.total_size);
	assert (i);
      }

    if (success)
      {
	if (ctx.ptr != ctx.end)
	  /* Did we read up everything?  */
	  {
	    where wh = WHERE (sec_info, NULL);
	    wr_message (cat (mc_die_other, mc_impact_4), &wh,
			": CU lengths don't exactly match Elf_Data contents.");
	  }
	else
	  /* Did we consume all the relocations?  */
	  relocation_skip_rest (&sec->rel, sec->id);

	/* If we managed to read up everything, now do abbrev usage
	   analysis.  */
	for (struct abbrev_table *abbrevs = abbrev_chain;
	     abbrevs != NULL; abbrevs = abbrevs->next)
	  if (abbrevs->used && !abbrevs->skip_check)
	    for (size_t i = 0; i < abbrevs->size; ++i)
	      if (!abbrevs->abbr[i].used)
		wr_message (mc_impact_3 | mc_acc_bloat | mc_abbrevs,
			    &abbrevs->abbr[i].where,
			    ": abbreviation is never used.\n");
      }


    /* We used to check that all CUs have the same address size.  Now
       that we validate address_size of each CU against the ELF header,
       that's not necessary anymore.  */

    bool references_sound = check_global_die_references (cu_chain);
    ref_record_free (&die_refs);

    if (strings_coverage != NULL)
      {
	if (success)
	  {
	    struct hole_info info = {sec_str, mc_strings, strings->d_buf, 0};
	    coverage_find_holes (strings_coverage, 0, strings->d_size,
				 found_hole, &info);
	  }
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
}

read_cu_headers::read_cu_headers (dwarflint &lint)
  : _m_sec_info (lint.check (_m_sec_info))
  , cu_headers (read_info_headers (&_m_sec_info->file,
				   &_m_sec_info->sect,
				   _m_sec_info->reldata ()))
{
}

check_debug_info::check_debug_info (dwarflint &lint)
  : _m_sec_info (lint.check (_m_sec_info))
  , _m_sec_abbrev (lint.check (_m_sec_abbrev))
  , _m_sec_str (lint.check (_m_sec_str))
  , _m_abbrevs (lint.check (_m_abbrevs))
  , _m_cu_headers (lint.check (_m_cu_headers))
{
  memset (&cu_cov, 0, sizeof (cu_cov));

  cu *chain = check_info_structural
    (&_m_sec_info->file, &_m_sec_info->sect,
     &_m_abbrevs->abbrevs.begin ()->second,
     _m_sec_str->sect.data, &cu_cov,
     _m_cu_headers->cu_headers);

  if (chain == NULL)
    throw check_base::failed ();

  for (cu *cu = chain; cu != NULL; cu = cu->next)
    cus.push_back (*cu);

  // re-link CUs so that they form a chain again.  This is to
  // interface with C-level code.  The last CU's next is null, so we
  // don't have to re-link it.
  cu *last = NULL;
  for (std::vector<cu>::iterator it = cus.begin ();
       it != cus.end (); ++it)
    {
      cu *cur = &*it;
      if (last != NULL)
	last->next = cur;
      last = cur;
    }

  if (cus.size () > 0)
    assert (cus.back ().next == NULL);
}

check_debug_info::~check_debug_info ()
{
  cu_free (&cus.back ());
}
