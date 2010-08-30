/* Low-level checking of .debug_pub*.
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

#include "check_debug_info.hh"
#include "sections.hh"
#include "pri.hh"

namespace
{
  template<section_id sec_id>
  class check_debug_pub
    : public check<check_debug_pub<sec_id> >
  {
    typedef section<sec_id> section_t;
    section_t *_m_sec;
    elf_file const &_m_file;

    bool check_pub_structural (check_debug_info *cus);

  public:
    static checkdescriptor const &descriptor () {
      static std::string name
	= (std::string)"check_"
	+ (section_t::descriptor ().name () + 1);
      static checkdescriptor cd
	(checkdescriptor::create (name.c_str ())
	 .prereq<typeof (*_m_sec)> ());
      return cd;
    }

    check_debug_pub (checkstack &stack, dwarflint &lint)
      : _m_sec (lint.check (stack, _m_sec))
      , _m_file (_m_sec->file)
    {
      check_pub_structural (lint.toplev_check<check_debug_info> (stack));
    }
  };

  template <class A, class B>
  struct where xwhere (A a, B b)
  {
    return WHERE (a, b);
  }
}

template <section_id sec_id>
bool
check_debug_pub<sec_id>::check_pub_structural (check_debug_info *cus)
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
      if (cus != NULL && (cu = cus->find_cu (cu_offset)) == NULL)
	wr_error (where)
	  << "unresolved reference to " << pri::CU (cu_offset)
	  << '.' << std::endl;
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
	if (sub_ctx.ptr != sub_ctx.end
	    && !check_zero_padding (&sub_ctx, mc_pubtables, &wh))
	  {
	    wh = WHERE (_m_sec->sect.id, NULL);
	    wr_message_padding_n0 (mc_pubtables | mc_error, &wh,
				   read_ctx_get_offset (&sub_ctx),
				   read_ctx_get_offset (&sub_ctx) + size);
	    retval = false;
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

namespace
{
  reg<check_debug_pub<sec_pubnames> > reg_debug_pubnames;
  reg<check_debug_pub<sec_pubtypes> > reg_debug_pubtypes;
}
