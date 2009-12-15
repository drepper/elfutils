/* Low-level checking of .debug_line.
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

#include <dwarf.h>
#include "../libdw/known-dwarf.h"
#include "dwarfstrings.h"

#include <sstream>

namespace
{
  class check_debug_line
    : public check<check_debug_line>
  {
    section<sec_line> *_m_sec;

    struct include_directory_t
    {
      std::string name;
      bool used;
    };
    typedef std::vector<include_directory_t> include_directories_t;

    struct file_t
    {
      const char *name;
      uint64_t dir_idx;
      bool used;
    };
    typedef std::vector<file_t> files_t;

  public:
    explicit check_debug_line (dwarflint &lint);

    /* Directory index.  */
    bool read_directory_index (include_directories_t &include_directories,
			       files_t &files, read_ctx *ctx,
			       const char *name, uint64_t *ptr,
			       where *where, bool &retval)
    {
      size_t nfile = files.size () + 1;
      if (!checked_read_uleb128 (ctx, ptr,
				 where, "directory index"))
	return false;

      if (*name == '/' && *ptr != 0)
	wr_message (*where, cat (mc_impact_2, mc_line, mc_header))
	  << "file #" << nfile
	  << " has absolute pathname, but refers to directory != 0."
	  << std::endl;

      if (*ptr > include_directories.size ())
	/* Not >=, dirs are indexed from 1.  */
	{
	  wr_message (*where, cat (mc_impact_4, mc_line, mc_header))
	    << "file #" << nfile
	    << " refers to directory #" << *ptr
	    << ", which wasn't defined." << std::endl;

	  /* Consumer might choke on that.  */
	  retval = false;
	}
      else if (*ptr != 0)
	include_directories[*ptr - 1].used = true;
      return true;
    }

    bool
    use_file (files_t &files, uint64_t file_idx,
	      where *where, char const *msg = "")
    {
      if (file_idx == 0 || file_idx > files.size ())
	{
	  wr_error (*where)
	    << msg << "invalid file index " << file_idx << '.'
	    << std::endl;
	  return false;
	}
      else
	files[file_idx - 1].used = true;
      return true;
    }
  };

  reg<check_debug_line> reg_debug_line;
}

check_debug_line::check_debug_line (dwarflint &lint)
  : _m_sec (lint.check (_m_sec))
{
  check_debug_info *cus = lint.toplev_check<check_debug_info> ();

  addr_record line_tables;
  WIPE (line_tables);

  bool addr_64 = _m_sec->file.addr_64;
  struct read_ctx ctx;
  read_ctx_init (&ctx, _m_sec->sect.data, _m_sec->file.other_byte_order);

  // For violations that the high-level might not handle.
  bool success = true;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (_m_sec->sect.id, NULL);
      uint64_t set_offset = read_ctx_get_offset (&ctx);
      where_reset_1 (&where, set_offset);
      addr_record_add (&line_tables, set_offset);
      const unsigned char *set_begin = ctx.ptr;

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      int offset_size;
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (where) << "can't read table length." << std::endl;
	  throw check_base::failed ();
	}
      if (!read_size_extra (&ctx, size32, &size, &offset_size, &where))
	throw check_base::failed ();

      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, set_begin, set_end))
	{
	not_enough:
	  wr_error (where)
	    << pri::not_enough ("next unit") << '.' << std::endl;
	  throw check_base::failed ();
	}
      sub_ctx.ptr = ctx.ptr;
      sub_ctx.begin = ctx.begin;

      {
      /* Version.  */
      uint16_t version;
      if (!read_ctx_read_2ubyte (&sub_ctx, &version))
	{
	  wr_error (where) << "can't read set version." << std::endl;
	skip:
	  success = false;
	  goto next;
	}
      if (!supported_version (version, 2, &where, 2, 3))
	goto skip;

      /* Header length.  */
      uint64_t header_length;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &header_length))
	{
	  wr_error (where) << "can't read attribute value." << std::endl;
	  goto skip;
	}
      const unsigned char *program_start = sub_ctx.ptr + header_length;

      /* Minimum instruction length.  */
      uint8_t minimum_i_length;
      if (!read_ctx_read_ubyte (&sub_ctx, &minimum_i_length))
	{
	  wr_error (where)
	    << "can't read minimum instruction length." << std::endl;
	  goto skip;
	}

      /* Default value of is_stmt.  */
      uint8_t default_is_stmt;
      if (!read_ctx_read_ubyte (&sub_ctx, &default_is_stmt))
	{
	  wr_error (where) << "can't read default_is_stmt." << std::endl;
	  goto skip;
	}
      /* 7.21: The boolean values "true" and "false" used by the line
	 number information program are encoded as a single byte
	 containing the value 0 for "false," and a non-zero value for
	 "true."  [But give a notice if it's not 0 or 1.]  */
      if (default_is_stmt != 0
	  && default_is_stmt != 1)
	wr_message (where, cat (mc_line, mc_impact_2, mc_header))
	  << "default_is_stmt should be 0 or 1, not "
	  << default_is_stmt << '.' << std::endl;

      /* Line base.  */
      int8_t line_base;
      if (!read_ctx_read_ubyte (&sub_ctx, (uint8_t *)&line_base))
	{
	  wr_error (where) << "can't read line_base." << std::endl;
	  goto skip;
	}

      /* Line range.  */
      uint8_t line_range;
      if (!read_ctx_read_ubyte (&sub_ctx, &line_range))
	{
	  wr_error (where) << "can't read line_range." << std::endl;
	  goto skip;
	}

      /* Opcode base.  */
      uint8_t opcode_base;
      if (!read_ctx_read_ubyte (&sub_ctx, &opcode_base))
	{
	  wr_error (where) << "can't read opcode_base." << std::endl;
	  goto skip;
	}

      /* Standard opcode lengths.  */
      if (opcode_base == 0)
	{
	  wr_error (where) << "opcode base set to 0." << std::endl;
	  opcode_base = 1; // so that in following, our -1s don't underrun
	}
      uint8_t std_opc_lengths[opcode_base - 1]; /* -1, opcodes go from 1.  */
      for (unsigned i = 0; i < (unsigned)(opcode_base - 1); ++i)
	if (!read_ctx_read_ubyte (&sub_ctx, std_opc_lengths + i))
	  {
	    wr_error (where)
	      << "can't read length of standard opcode #" << i << '.'
	      << std::endl;
	    goto skip;
	  }

      include_directories_t include_directories;
      while (!read_ctx_eof (&sub_ctx))
	{
	  const char *name = read_ctx_read_str (&sub_ctx);
	  if (name == NULL)
	    {
	      wr_error (where)
		<< "can't read name of include directory #"
		<< include_directories.size () + 1 // Numbered from 1.
		<< '.' << std::endl;
	      goto skip;
	    }
	  if (*name == 0)
	    break;

	  include_directories.push_back ((include_directory_t){name, false});
	}

      /* File names.  */
      files_t files;
      while (1)
	{
	  const char *name = read_ctx_read_str (&sub_ctx);
	  if (name == NULL)
	    {
	      wr_error (where)
		<< "can't read name of file #"
		<< files.size () + 1 // Numbered from 1.
		<< '.' << std::endl;
	      goto skip;
	    }
	  if (*name == 0)
	    break;

	  uint64_t dir_idx;
	  if (!read_directory_index (include_directories, files,
				     &sub_ctx, name, &dir_idx, &where, success))
	    goto skip;

	  /* Time of last modification.  */
	  uint64_t timestamp;
	  if (!checked_read_uleb128 (&sub_ctx, &timestamp,
				     &where, "timestamp of file entry"))
	    goto skip;

	  /* Size of the file.  */
	  uint64_t file_size;
	  if (!checked_read_uleb128 (&sub_ctx, &file_size,
				     &where, "file size of file entry"))
	    goto skip;

	  files.push_back ((struct file_t){name, dir_idx, false});
	}

      /* Now that we have table of filenames, validate DW_AT_decl_file
	 references.  We don't include filenames defined through
	 DW_LNE_define_file in consideration.  */

      if (cus != NULL)
	{
	  bool found = false;
	  for (std::vector<cu>::const_iterator it = cus->cus.begin ();
	       it != cus->cus.end (); ++it)
	    if (it->stmt_list.addr == set_offset)
	      {
		found = true;
		for (size_t i = 0; i < it->decl_file_refs.size; ++i)
		  if (!use_file (files,
				 it->decl_file_refs.refs[i].addr,
				 &it->decl_file_refs.refs[i].who))
		    success = false;
	      }
	  if (!found)
	    wr_message (where, mc_line)
	      << "no CU uses this line table." << std::endl;
	}

      /* Skip the rest of the header.  */
      if (sub_ctx.ptr > program_start)
	{
	  wr_error (where)
	    << "header claims that it has a size of " << header_length
	    << ", but in fact it has a size of "
	    << (sub_ctx.ptr - program_start + header_length)
	    << '.' << std::endl;

	  /* Assume that the header lies, and what follows is in
	     fact line number program.  */
	  success = false;
	}
      else if (sub_ctx.ptr < program_start)
	{
	  struct where wh = WHERE (sec_line, NULL);
	  if (!check_zero_padding (&sub_ctx, cat (mc_line, mc_header), &where))
	    wr_message_padding_n0 (cat (mc_line, mc_header), &wh,
				   read_ctx_get_offset (&sub_ctx),
				   program_start - sub_ctx.begin);
	  sub_ctx.ptr = program_start;
	}

      bool terminated = false;
      bool first_file = true;
      bool seen_opcode = false;
      while (!read_ctx_eof (&sub_ctx))
	{
	  where_reset_2 (&where, read_ctx_get_offset (&sub_ctx));
	  uint8_t opcode;
	  if (!read_ctx_read_ubyte (&sub_ctx, &opcode))
	    {
	      wr_error (where) << "can't read opcode." << std::endl;
	      goto skip;
	    }

	  unsigned operands = 0;
	  uint8_t extended = 0;
	  switch (opcode)
	    {
	      /* Extended opcodes.  */
	    case 0:
	      {
		uint64_t skip_len;
		if (!checked_read_uleb128 (&sub_ctx, &skip_len, &where,
					   "length of extended opcode"))
		  goto skip;
		const unsigned char *next = sub_ctx.ptr + skip_len;
		if (!read_ctx_read_ubyte (&sub_ctx, &extended))
		  {
		    wr_error (where)
		      << "can't read extended opcode." << std::endl;
		    goto skip;
		  }

		bool handled = true;
		switch (extended)
		  {
		  case DW_LNE_end_sequence:
		    terminated = true;
		    break;

		  case DW_LNE_set_address:
		    {
		      uint64_t ctx_offset = read_ctx_get_offset (&sub_ctx);
		      uint64_t addr;
		      if (!read_ctx_read_offset (&sub_ctx, addr_64, &addr))
			{
			  wr_error (where)
			    << "can't read operand of DW_LNE_set_address."
			    << std::endl;
			  goto skip;
			}

		      struct relocation *rel;
		      if ((rel = relocation_next (&_m_sec->sect.rel, ctx_offset,
						  &where, skip_mismatched)))
			relocate_one (&_m_sec->file, &_m_sec->sect.rel, rel,
				      addr_64 ? 8 : 4,
				      &addr, &where, rel_address, NULL);
		      else if (_m_sec->file.ehdr.e_type == ET_REL)
			wr_message (where, cat (mc_impact_2, mc_line, mc_reloc))
			  << pri::lacks_relocation ("DW_LNE_set_address")
			  << '.' << std::endl;
		      break;
		    }

		  case DW_LNE_define_file:
		    {
		      const char *name;
		      if ((name = read_ctx_read_str (&sub_ctx)) == NULL)
			{
			  wr_error (where)
			    << "can't read filename operand of DW_LNE_define_file."
			    << std::endl;
			  goto skip;
			}
		      uint64_t dir_idx;
		      if (!read_directory_index (include_directories,
						 files, &sub_ctx, name,
						 &dir_idx, &where, success))
			goto skip;
		      files.push_back
			((struct file_t){name, dir_idx, false});
		      operands = 2; /* Skip mtime & size of the file.  */
		    }

		    /* See if we know about any other standard opcodes.  */
		  default:
		    handled = false;
		    switch (extended)
		      {
#define ONE_KNOWN_DW_LNE(NAME, CODE) case CODE: break;
			ALL_KNOWN_DW_LNE
#undef ONE_KNOWN_DW_LNE
		      default:
			/* No we don't, emit a warning.  */
			wr_message (where, cat (mc_impact_2, mc_line))
			  << "unknown extended opcode #" << extended
			  << '.' << std::endl;
		      };
		  };

		if (sub_ctx.ptr > next)
		  {
		    wr_error (where)
		      << "opcode claims that it has a size of " << skip_len
		      << ", but in fact it has a size of "
		      << (skip_len + (next - sub_ctx.ptr)) << '.' << std::endl;
		    success = false;
		  }
		else if (sub_ctx.ptr < next)
		  {
		    if (handled
			&& !check_zero_padding (&sub_ctx, mc_line, &where))
		      {
			struct where wh = WHERE (sec_line, NULL);
			wr_message_padding_n0 (mc_line, &wh,
					       read_ctx_get_offset (&sub_ctx),
					       next - sub_ctx.begin);
		      }
		    sub_ctx.ptr = next;
		  }
		break;
	      }

	      /* Standard opcodes that need validation or have
		 non-ULEB operands.  */
	    case DW_LNS_fixed_advance_pc:
	      {
		uint16_t a;
		if (!read_ctx_read_2ubyte (&sub_ctx, &a))
		  {
		    wr_error (where)
		      << "can't read operand of DW_LNS_fixed_advance_pc."
		      << std::endl;
		    goto skip;
		  }
		break;
	      }

	    case DW_LNS_set_file:
	      {
		uint64_t file_idx;
		if (!checked_read_uleb128 (&sub_ctx, &file_idx, &where,
					   "DW_LNS_set_file operand"))
		  goto skip;
		if (!use_file (files, file_idx, &where, "DW_LNS_set_file: "))
		  success = false;
		first_file = false;
	      }
	      break;

	    case DW_LNS_set_isa:
	      // XXX is it possible to validate this?
	      operands = 1;
	      break;

	      /* All the other opcodes.  */
	    default:
	      if (opcode < opcode_base)
		operands = std_opc_lengths[opcode - 1];

	      switch (opcode)
		{
#define ONE_KNOWN_DW_LNS(NAME, CODE) case CODE: break;
		  ALL_KNOWN_DW_LNS
#undef ONE_KNOWN_DW_LNS

		default:
		  if (opcode < opcode_base)
		    wr_message (where, cat (mc_impact_2, mc_line))
		      << "unknown standard opcode #" << opcode
		      << '.' << std::endl;
		};
	    };

	  for (unsigned i = 0; i < operands; ++i)
	    {
	      uint64_t operand;
	      char buf[128];
	      if (opcode != 0)
		sprintf (buf, "operand #%d of DW_LNS_%s",
			 i, dwarf_line_standard_opcode_string (opcode));
	      else
		sprintf (buf, "operand #%d of extended opcode %d",
			 i, extended);
	      if (!checked_read_uleb128 (&sub_ctx, &operand, &where, buf))
		goto skip;
	    }

	  if (first_file)
	    {
	      if (!use_file (files, 1, &where,
			     "initial value of `file' register: "))
		success = false;
	      first_file = false;
	    }

	  if (opcode != 0 || extended != DW_LNE_end_sequence)
	    seen_opcode = true;
	}

      for (size_t i = 0; i < include_directories.size (); ++i)
	if (!include_directories[i].used)
	  wr_message (where,
		      cat (mc_impact_3, mc_acc_bloat, mc_line, mc_header))
	    << "the include #" << i + 1
	    << " `" << include_directories[i].name
	    << "' is not used." << std::endl;

      if (cus != NULL)
	// We can't do full analysis unless we know which DIEs refer
	// to files.
	for (size_t i = 0; i < files.size (); ++i)
	  if (!files[i].used)
	    wr_message (where,
			cat (mc_impact_3, mc_acc_bloat, mc_line, mc_header))
	      << "the file #" << i + 1
	      << " `" << files[i].name << "' is not used." << std::endl;

      if (!seen_opcode)
	wr_message (where, cat (mc_line, mc_acc_bloat, mc_impact_3))
	  << "empty line number program." << std::endl;

      struct where wh = WHERE (sec_line, NULL);
      if (!terminated)
	wr_error (where)
	  << "sequence of opcodes not terminated with DW_LNE_end_sequence."
	  << std::endl;
      else if (sub_ctx.ptr != sub_ctx.end
	       && !check_zero_padding (&sub_ctx, mc_line, &wh))
	wr_message_padding_n0 (mc_line, &wh,
			       /*begin*/read_ctx_get_offset (&sub_ctx),
			       /*end*/sub_ctx.end - sub_ctx.begin);
      }

      /* XXX overlaps in defined addresses are probably OK, one
	 instruction can be derived from several statements.  But
	 certain flags in table should be consistent in that case,
	 namely is_stmt, basic_block, end_sequence, prologue_end,
	 epilogue_begin, isa.  */

    next:
      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (success)
    relocation_skip_rest (&_m_sec->sect.rel, _m_sec->sect.id);
  else
    throw check_base::failed ();

  check_debug_info *info = NULL;
  info = lint.toplev_check (info);
  if (info != NULL)
    for (std::vector<cu>::iterator it = info->cus.begin ();
	 it != info->cus.end (); ++it)
      if (it->stmt_list.addr != (uint64_t)-1
	  && !addr_record_has_addr (&line_tables, it->stmt_list.addr))
	wr_error (it->stmt_list.who)
	  << "unresolved reference to .debug_line table "
	  << pri::hex (it->stmt_list.addr) << '.' << std::endl;
  addr_record_free (&line_tables);
}
