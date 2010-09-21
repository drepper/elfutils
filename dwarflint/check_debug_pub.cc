/* Low-level checking of .debug_pub*.
   Copyright (C) 2009, 2010 Red Hat, Inc.
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

#include "check_debug_pub.hh"
#include "check_debug_info.hh"
#include "sections.hh"
#include "pri.hh"

namespace
{
  template <class A, class B>
  struct where xwhere (A a, B b)
  {
    return WHERE (a, b);
  }
}

template<section_id sec_id>
check_debug_pub<sec_id>::check_debug_pub (checkstack &stack, dwarflint &lint)
  : _m_sec (lint.check (stack, _m_sec))
  , _m_file (_m_sec->file)
  , _m_cus (lint.toplev_check (stack, _m_cus))
{
  check_pub_structural ();
}


checkdescriptor const *
check_debug_pubnames::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_pubnames")
     .groups ("@low")
     .prereq<typeof (*_m_sec)> ()
     .prereq<check_debug_info> ()
     .description (
"Checks for low-level structure of .debug_pubnames.  In addition it\n"
"checks:\n"
" - for garbage inside padding\n"
" - that relocations are valid.  In ET_REL files that certain fields\n"
"   are relocated\n"
"Furthermore, if .debug_info is valid, it is checked:\n"
" - that references point to actual CUs and DIEs\n"
" - that there's only one pub section per CU\n"));
  return &cd;
}
template check_debug_pub<sec_pubnames>::check_debug_pub (checkstack &stack,
							 dwarflint &lint);

checkdescriptor const *
check_debug_pubtypes::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_pubtypes")
     .groups ("@low")
     .prereq<typeof (*_m_sec)> ()
     .prereq<check_debug_info> ()
     .description (
"Checks for low-level structure of .debug_pubtypes.  In addition it\n"
"makes the same checks as check_debug_pubnames.\n"));
  return &cd;
}
template check_debug_pub<sec_pubtypes>::check_debug_pub (checkstack &stack,
							 dwarflint &lint);

template <section_id sec_id>
bool
check_debug_pub<sec_id>::check_pub_structural ()
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, _m_sec->sect.data, _m_file.other_byte_order);
  bool retval = true;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (_m_sec->sect.id, NULL);
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

      {
      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, set_begin, set_end))
	goto not_enough;
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
      if ((rel = relocation_next (&_m_sec->sect.rel, ctx_offset,
				  &where, skip_mismatched)))
	relocate_one (&_m_file, &_m_sec->sect.rel, rel, offset_size,
		      &cu_offset, &where, sec_info, NULL);
      else if (_m_file.ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_pubtables | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (_m_cus != NULL && (cu = _m_cus->find_cu (cu_offset)) == NULL)
	wr_error (where)
	  << "unresolved reference to " << pri::CU (cu_offset)
	  << '.' << std::endl;
      // xxx this can be checked even without CU
      if (cu != NULL)
	{
	  where.ref = &cu->head->where;
	  bool *has = _m_sec->sect.id == sec_pubnames
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
	  wr_error (where)
	    << "the table covers length " << cu_len << " but CU has length "
	    << cu->head->total_size << '.' << std::endl;
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
	      wr_error (where)
		<< "unresolved reference to " << pri::DIE (offset)
		<< '.' << std::endl;
	      retval = false;
	      goto next;
	    }

	  // xxx read_ctx_read_str???
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

	struct where wh = WHERE (_m_sec->sect.id, NULL);
	if (sub_ctx.ptr != sub_ctx.end)
	  {
	    uint64_t off_start, off_end;
	    if (read_check_zero_padding (&sub_ctx, &off_start, &off_end))
	      wr_message_padding_0 (mc_pubtables, &wh, off_start, off_end);
	    else
	      {
		wr_message_padding_n0 (mc_pubtables | mc_error, &wh,
				       off_start, off_start + size);
		retval = false;
	      }
	  }
      }

    next:
      if (read_ctx_skip (&ctx, size))
	continue;

    not_enough:
      wr_error (&where, PRI_NOT_ENOUGH, "next set");
      return false;
    }

  if (retval)
    relocation_skip_rest (&_m_sec->sect.rel, _m_sec->sect.id);

  return retval;
}
