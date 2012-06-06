/* Low-level checking of .debug_aranges.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>

#include "elf_file.hh"
#include "sections.hh"
#include "check_debug_aranges.hh"
#include "check_debug_info.hh"
#include "check_debug_loc_range.hh"
#include "cu_coverage.hh"
#include "checked_read.hh"
#include "misc.hh"
#include "pri.hh"

char const *
locus_simple_fmt::cudie_n ()
{
  return "CU DIE";
}

std::string
arange_locus::format (bool brief) const
{
  std::stringstream ss;
  if (!brief)
    ss << section_name[sec_aranges] << ": ";

  if (_m_arange_offset != (Dwarf_Off)-1)
    ss << "arange 0x" << std::hex << _m_arange_offset;
  else if (_m_table_offset != (Dwarf_Off)-1)
    ss << "table " << std::dec << _m_table_offset;
  else
    ss << "arange";

  if (_m_cudie_locus != NULL)
    ss << " (" << _m_cudie_locus->format (true) << ')';

  return ss.str ();
}

checkdescriptor const *
check_debug_aranges::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_aranges")
     .groups ("@low")
     .description (
"Checks for low-level structure of .debug_aranges.  In addition it "
"checks:\n"
" - that relocations are valid.  In ET_REL files that certain fields "
"are relocated\n"
" - for dangling and duplicate CU references\n"
" - for garbage inside padding\n"
" - for zero-length ranges\n"
" - that the ranges cover all the address range covered by CUs\n"
		   ));
  return &cd;
}

static reg<check_debug_aranges> reg_debug_aranges;

static struct cu *
cu_find_cu (struct cu *cu_chain, uint64_t offset)
{
  for (struct cu *it = cu_chain; it != NULL; it = it->next)
    if (it->head->offset == offset)
      return it;
  return NULL;
}

#define PRI_CU "CU 0x%" PRIx64

namespace
{
  struct hole_user
  {
    elf_file *elf;
    section_id id;
    char const *what;
    bool reverse;

    hole_user (elf_file *a_elf, section_id a_id,
	       char const *a_what, bool a_reverse)
      : elf (a_elf)
      , id (a_id)
      , what (a_what)
      , reverse (a_reverse)
    {}
  };
}

static bool
hole (uint64_t start, uint64_t length, void *user)
{
  /* We need to check alignment vs. the covered section.  Find
     where the hole lies.  */
  ::hole_user &info = *static_cast< ::hole_user *> (user);
  struct elf_file *elf = info.elf;
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
      char const *what = info.what;
      char const *cu = "CU DIEs";
      if (info.reverse)
	{
	  char const *tmp = what;
	  what = cu;
	  cu = tmp;
	}
      wr_message (section_locus (info.id), mc_aranges | mc_impact_3)
	<< "addresses " << range_fmt (buf, sizeof (buf), start, start + length)
	<< " are covered with " << cu << ", but not with " << what << "."
	<< std::endl;
    }

  if (sec == NULL)
    wr_error (NULL, "Couldn't find the section containing the above hole.\n");

  return true;
}

static void
compare_coverage_1 (struct elf_file *file,
		    struct coverage *coverage, struct coverage *other,
		    enum section_id id, char const *what,
		    bool reverse)
{
  struct coverage cov = *coverage - *other;
  hole_user info (file, id, what, reverse);
  cov.find_ranges (hole, &info);
}

static void
compare_coverage (struct elf_file *file,
		  struct coverage *coverage, struct coverage *other,
		  enum section_id id, char const *what)
{
  compare_coverage_1 (file, coverage, other, id, what, false);
  compare_coverage_1 (file, other, coverage, id, what, true);
}

inline static void
aranges_coverage_add (struct coverage *aranges_coverage,
		      uint64_t begin, uint64_t length,
		      locus const &loc)
{
  if (aranges_coverage->is_overlap (begin, length))
    {
      char buf[128];
      /* Not a show stopper, this shouldn't derail high-level.  */
      wr_message (loc, mc_aranges | mc_impact_2 | mc_error)
	<< "the range " << range_fmt (buf, sizeof buf, begin, begin + length)
	<< " overlaps with another one." << std::endl;
    }

  aranges_coverage->add (begin, length);
}

/* COVERAGE is portion of address space covered by CUs (either via
   low_pc/high_pc pairs, or via DW_AT_ranges references).  If
   non-NULL, analysis of arange coverage is done against that set. */
static bool
check_aranges_structural (struct elf_file *file,
			  struct sec *sec,
			  struct cu *cu_chain,
			  struct coverage *coverage)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  bool retval = true;

  struct coverage *aranges_coverage
    = coverage != NULL ? new struct coverage () : NULL;

  while (!read_ctx_eof (&ctx))
    {
      arange_locus where (read_ctx_get_offset (&ctx));
      const unsigned char *atab_begin = ctx.ptr;

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      int offset_size;
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (&where, ": can't read table length.\n");
	  return false;
	}
      if (!read_size_extra (&ctx, size32, &size, &offset_size, where))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *atab_end = ctx.ptr + size;
      if (false)
	{
	next:
	  if (!read_ctx_skip (&ctx, size))
	    /* A "can't happen" error.  */
	    goto not_enough;
	  continue;
	}
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
      if (!supported_version (version, 1, where, 2))
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
				  where, skip_mismatched)))
	relocate_one (file, &sec->rel, rel, offset_size,
		      &cu_offset, where, sec_info, NULL);
      else if (file->ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_aranges | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (cu_chain != NULL && (cu = cu_find_cu (cu_chain, cu_offset)) == NULL)
	wr_error (&where, ": unresolved reference to " PRI_CU ".\n", cu_offset);

      cudie_locus cudie_loc (cu != NULL ? cu->cudie_offset : -1);
      if (cu != NULL)
	{
	  where.set_cudie (&cudie_loc);
	  if (cu->has_arange)
	    wr_error (where)
	      << "there has already been arange section for this CU."
	      << std::endl;
	  else
	    cu->has_arange = true;
	}

      /* Address size.  */
      int address_size;
      error_code err = read_address_size (&sub_ctx, file->addr_64,
					  &address_size, where);
      if (err != err_ok)
	retval = false;
      if (err == err_fatal)
	goto next;

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
	  wr_error (&where, ": dwarflint can't handle segment_size != 0.\n");
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
	  where.set_arange (read_ctx_get_offset (&sub_ctx));

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
				      where, skip_mismatched)))
	    {
	      address_relocated = true;
	      relocate_one (file, &sec->rel, rel, address_size,
			    &address, where, rel_target::rel_address, NULL);
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
	  else if (retval && aranges_coverage != NULL)
	    aranges_coverage_add (aranges_coverage, address, length, where);
	}

      if (sub_ctx.ptr != sub_ctx.end)
	{
	  uint64_t start, end;
	  section_locus wh (sec_aranges);
	  if (read_check_zero_padding (&sub_ctx, &start, &end))
	    wr_message_padding_0 (mc_aranges, wh, start, end);
	  else
	    {
	      wr_message_padding_n0 (mc_aranges | mc_error, wh,
				     start, start + size);
	      retval = false;
	    }
	}

      goto next;
    }

  if (aranges_coverage != NULL)
    {
      compare_coverage (file, coverage, aranges_coverage,
			sec_aranges, "aranges");
      delete aranges_coverage;
    }

  return retval;
}

check_debug_aranges::check_debug_aranges (checkstack &stack, dwarflint &lint)
  : _m_sec_aranges (lint.check (stack, _m_sec_aranges))
  , _m_info (lint.toplev_check (stack, _m_info))
  , _m_cu_coverage (lint.toplev_check (stack, _m_cu_coverage))
{
  coverage *cov = _m_cu_coverage != NULL ? &_m_cu_coverage->cov : NULL;

  if (!check_aranges_structural (&_m_sec_aranges->file,
				 &_m_sec_aranges->sect,
				 _m_info != NULL
				   ? &_m_info->cus.front () : NULL,
				 cov))
    throw check_base::failed ();
}
