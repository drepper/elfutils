/* Low-level checking of .debug_line.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "check_debug_line.hh"
#include "check_debug_info.hh"
#include "sections.hh"
#include "pri.hh"
#include "checked_read.hh"
#include "messages.hh"
#include "misc.hh"

#include <dwarf.h>
#include "../libdw/known-dwarf.h"
#include "../src/dwarfstrings.h"

#include <sstream>

checkdescriptor const *
check_debug_line::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_line")
     .groups ("@low")
     .schedule (false)
     .description (
"Checks for low-level structure of .debug_line.  In addition it\n"
"checks:\n"
" - for normalized values of certain attributes (such as that "
"default_is_stmt is 0 or 1, even though technically any non-zero "
"value is allowed).\n"
" - for valid setting of opcode base (i.e. non-zero) and any file"
"indices\n"
" - that all include directories and all files are used\n"
" - that files with absolute paths don't refer to include directories,"
"and otherwise that the directory reference is valid\n"
" - that each used standard or extended opcode is known (note that this "
"assumes that elfutils know about all opcodes used in practice.  Be "
"sure to build against recent-enough version).\n"
" - that the line number program is properly terminated with the "
"DW_LNE_end_sequence instruction and that it contains at least one "
"other instruction\n"
" - that relocations are valid.  In ET_REL files that certain fields "
"are relocated\n"
"Furthermore, if .debug_info is valid, it is checked that each line "
"table is used by some CU.\n"
"TODOs:\n"
" - overlaps in defined addresses are probably OK, one instruction can "
"be derived from several statements.  But certain flags in table "
"should be consistent in that case, namely is_stmt, basic_block, "
"end_sequence, prologue_end, epilogue_begin, isa.\n"
		   ));
  return &cd;
}

static reg<check_debug_line> reg_debug_line;

namespace
{
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

  /* Directory index.  */
  bool read_directory_index (include_directories_t &include_directories,
			     files_t &files, read_ctx *ctx,
			     const char *name, uint64_t *ptr,
			     locus const &loc, bool &retval)
  {
    size_t nfile = files.size () + 1;
    if (!checked_read_uleb128 (ctx, ptr,
			       loc, "directory index"))
      return false;

    if (*name == '/' && *ptr != 0)
      wr_message (loc, mc_impact_2 | mc_line | mc_header)
	<< "file #" << nfile
	<< " has absolute pathname, but refers to directory != 0."
	<< std::endl;

    if (*ptr > include_directories.size ())
      /* Not >=, dirs are indexed from 1.  */
      {
	wr_message (loc, mc_impact_4 | mc_line | mc_header)
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
	    locus const &loc, char const *msg = "")
  {
    if (file_idx == 0 || file_idx > files.size ())
      {
	wr_error (loc)
	  << msg << "invalid file index " << file_idx << '.'
	  << std::endl;
	return false;
      }
    else
      files[file_idx - 1].used = true;
    return true;
  }
}

namespace
{
  char const *
  table_n ()
  {
    return "table";
  }

  typedef fixed_locus<sec_line, table_n,
		      locus_simple_fmt::dec> line_table_locus;
}

check_debug_line::check_debug_line (checkstack &stack, dwarflint &lint)
  : _m_sec (lint.check (stack, _m_sec))
  , _m_info (lint.toplev_check (stack, _m_info))
{
  bool addr_64 = _m_sec->file.addr_64;
  struct read_ctx ctx;
  read_ctx_init (&ctx, _m_sec->sect.data, _m_sec->file.other_byte_order);

  // For violations that the high-level might not handle.
  bool success = true;

  while (!read_ctx_eof (&ctx))
    {
      uint64_t set_offset = read_ctx_get_offset (&ctx);
      line_table_locus where (set_offset);
      _m_line_tables.insert ((Dwarf_Off)set_offset);
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
      if (!read_size_extra (&ctx, size32, &size, &offset_size, where))
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
      if (!supported_version (version, 2, where, 2, 3))
	goto skip;

      /* Header length.  */
      uint64_t header_length;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &header_length))
	{
	  wr_error (where) << "can't read attribute value." << std::endl;
	  goto skip;
	}
      const unsigned char *header_start = sub_ctx.ptr;

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
	wr_message (where, mc_line | mc_impact_2 | mc_header)
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
				     &sub_ctx, name, &dir_idx, where, success))
	    goto skip;

	  /* Time of last modification.  */
	  uint64_t timestamp;
	  if (!checked_read_uleb128 (&sub_ctx, &timestamp,
				     where, "timestamp of file entry"))
	    goto skip;

	  /* Size of the file.  */
	  uint64_t file_size;
	  if (!checked_read_uleb128 (&sub_ctx, &file_size,
				     where, "file size of file entry"))
	    goto skip;

	  files.push_back ((struct file_t){name, dir_idx, false});
	}

      /* Now that we have table of filenames, validate DW_AT_decl_file
	 references.  We don't include filenames defined through
	 DW_LNE_define_file in consideration.  */

      if (_m_info != NULL)
	{
	  bool found = false;
	  for (std::vector<cu>::const_iterator it = _m_info->cus.begin ();
	       it != _m_info->cus.end (); ++it)
	    if (it->stmt_list.addr == set_offset)
	      {
		found = true;
		for (ref_record::const_iterator
		       jt = it->decl_file_refs.begin ();
		     jt != it->decl_file_refs.end (); ++jt)
		  if (!use_file (files, jt->addr, jt->who))
		    success = false;
	      }
	  if (!found)
	    wr_message (where, mc_line)
	      << "no CU uses this line table." << std::endl;
	}

      const unsigned char *program_start = header_start + header_length;
      if (header_length > (uint64_t)(sub_ctx.end - header_start)
	  || sub_ctx.ptr > program_start)
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
	  /* Skip the rest of the header.  */
	  uint64_t off_start, off_end;
	  if (read_check_zero_padding (&sub_ctx, &off_start, &off_end))
	    wr_message_padding_0 (mc_line | mc_header, section_locus (sec_line),
				  off_start, off_end);
	  else
	    wr_message_padding_n0
	      (mc_line | mc_header, section_locus (sec_line),
	       off_start, program_start - sub_ctx.begin);
	  sub_ctx.ptr = program_start;
	}

      bool terminated = false;
      bool first_file = true;
      bool seen_opcode = false;
      while (!read_ctx_eof (&sub_ctx))
	{
	  section_locus op_where (sec_line, read_ctx_get_offset (&sub_ctx));
	  uint8_t opcode;
	  if (!read_ctx_read_ubyte (&sub_ctx, &opcode))
	    {
	      wr_error (op_where) << "can't read opcode." << std::endl;
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
		if (!checked_read_uleb128 (&sub_ctx, &skip_len, op_where,
					   "length of extended opcode"))
		  goto skip;
		if (!read_ctx_need_data (&sub_ctx, skip_len))
		  {
		    wr_error (op_where)
		      << "not enough data to read an opcode of length "
		      << skip_len << '.' << std::endl;
		    goto skip;
		  }

		const unsigned char *next = sub_ctx.ptr + skip_len;
		if (!read_ctx_read_ubyte (&sub_ctx, &extended))
		  {
		    wr_error (op_where)
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
			  wr_error (op_where)
			    << "can't read operand of DW_LNE_set_address."
			    << std::endl;
			  goto skip;
			}

		      struct relocation *rel;
		      if ((rel = relocation_next (&_m_sec->sect.rel, ctx_offset,
						  op_where, skip_mismatched)))
			relocate_one (&_m_sec->file, &_m_sec->sect.rel, rel,
				      addr_64 ? 8 : 4, &addr, op_where,
				      rel_target::rel_address, NULL);
		      else if (_m_sec->file.ehdr.e_type == ET_REL)
			{
			  wr_message (op_where,
				      mc_impact_2 | mc_line | mc_reloc)
			    << pri::lacks_relocation ("DW_LNE_set_address")
			    << '.' << std::endl;

			  // Don't do the addr checking in this case.
			  break;
			}

		      if (addr == 0)
			wr_message (op_where, mc_line | mc_impact_1)
			  << "DW_LNE_set_address with zero operand."
			  << std::endl;
		      break;
		    }

		  case DW_LNE_set_discriminator:
		    {
		      /* XXX Is there anything interesting we should
			 check here?  */
		      uint64_t disc;
		      if (!checked_read_uleb128 (&sub_ctx, &disc, op_where,
						 "set_discriminator operand"))
			goto skip;

		      /* The discriminator is reset to zero on any
			 sequence change.  So setting to zero is never
			 necessary.  */
		      if (disc == 0)
			wr_message (op_where, mc_line | mc_impact_1)
			  << "DW_LNE_set_discriminator with zero operand."
			  << std::endl;
		      break;
		    }

		  case DW_LNE_define_file:
		    {
		      const char *name;
		      if ((name = read_ctx_read_str (&sub_ctx)) == NULL)
			{
			  wr_error (op_where)
			    << "can't read filename operand of DW_LNE_define_file."
			    << std::endl;
			  goto skip;
			}
		      uint64_t dir_idx;
		      if (!read_directory_index (include_directories,
						 files, &sub_ctx, name,
						 &dir_idx, op_where, success))
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
#define DWARF_ONE_KNOWN_DW_LNE(NAME, CODE) case CODE: break;
			DWARF_ALL_KNOWN_DW_LNE
#undef DWARF_ONE_KNOWN_DW_LNE
		      default:
			/* No we don't, emit a warning.  */
			wr_message (op_where, mc_impact_2 | mc_line)
			  << "unknown extended opcode 0x"
			  << std::hex << +extended << std::dec
			  << '.' << std::endl;
		      };
		  };

		if (sub_ctx.ptr > next)
		  {
		    wr_error (op_where)
		      << "opcode claims that it has a size of " << skip_len
		      << ", but in fact it has a size of "
		      << (skip_len + (next - sub_ctx.ptr)) << '.' << std::endl;
		    success = false;
		  }
		else if (sub_ctx.ptr < next)
		  {
		    uint64_t off_start, off_end;
		    if (handled)
		      {
			if (read_check_zero_padding (&sub_ctx,
						     &off_start, &off_end))
			  wr_message_padding_0
			    (mc_line, section_locus (sec_line),
			     off_start, off_end);
			else
			  wr_message_padding_n0
			    (mc_line, section_locus (sec_line),
			     off_start, next - sub_ctx.begin);
		      }
		    sub_ctx.ptr = next;
		  }
		break;
	      }

	      /* Standard opcodes that need validation or have
		 non-ULEB operands.  */
	    case DW_LNS_advance_line:
	      {
		int64_t line_delta;
		if (!checked_read_sleb128 (&sub_ctx, &line_delta, op_where,
					   "DW_LNS_advance_line operand"))
		  goto skip;
	      }
	      break;

	    case DW_LNS_fixed_advance_pc:
	      {
		uint16_t a;
		if (!read_ctx_read_2ubyte (&sub_ctx, &a))
		  {
		    wr_error (op_where)
		      << "can't read operand of DW_LNS_fixed_advance_pc."
		      << std::endl;
		    goto skip;
		  }
		break;
	      }

	    case DW_LNS_set_file:
	      {
		uint64_t file_idx;
		if (!checked_read_uleb128 (&sub_ctx, &file_idx, op_where,
					   "DW_LNS_set_file operand"))
		  goto skip;
		if (!use_file (files, file_idx, op_where, "DW_LNS_set_file: "))
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
#define DWARF_ONE_KNOWN_DW_LNS(NAME, CODE) case CODE: break;
		  DWARF_ALL_KNOWN_DW_LNS
#undef DWARF_ONE_KNOWN_DW_LNS

		default:
		  if (opcode < opcode_base)
		    wr_message (op_where, mc_impact_2 | mc_line)
		      << "unknown standard opcode 0x"
		      << std::hex << +opcode << std::dec
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
		sprintf (buf, "operand #%d of DW_LNE_%s",
			 i, dwarf_line_extended_opcode_string (extended));
	      if (!checked_read_uleb128 (&sub_ctx, &operand, op_where, buf))
		goto skip;
	    }

	  if (first_file)
	    {
	      if (!use_file (files, 1, op_where,
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
		      mc_impact_3 | mc_acc_bloat | mc_line | mc_header)
	    << "the include #" << i + 1
	    << " `" << include_directories[i].name
	    << "' is not used." << std::endl;

      // We can't do full analysis unless we know which DIEs refer to
      // files.
      if (_m_info != NULL)
	{
	  bool useful = false;

	  for (size_t i = 0; i < files.size (); ++i)
	    if (!files[i].used)
	      wr_message (where,
			  mc_impact_3 | mc_acc_bloat | mc_line | mc_header)
		<< "the file #" << i + 1
		<< " `" << files[i].name << "' is not used." << std::endl;
	    else
	      useful = true;

	  if (!seen_opcode && !useful)
	    wr_message (where, mc_line | mc_acc_bloat | mc_impact_3)
	      << "empty line number program and no references from .debug_info."
	      << std::endl;
	}

      if (!terminated && seen_opcode)
	wr_error (where)
	  << "sequence of opcodes not terminated with DW_LNE_end_sequence."
	  << std::endl;
      else if (sub_ctx.ptr != sub_ctx.end)
	{
	  uint64_t off_start, off_end;
	  if (read_check_zero_padding (&sub_ctx, &off_start, &off_end))
	    wr_message_padding_0
	      (mc_line, section_locus (sec_line), off_start, off_end);
	  else
	    wr_message_padding_n0 (mc_line, section_locus (sec_line),
				   off_start, sub_ctx.end - sub_ctx.begin);
	}
      }

    next:
      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (success)
    relocation_skip_rest (&_m_sec->sect.rel, section_locus (_m_sec->sect.id));
  else
    throw check_base::failed ();
}

bool
check_debug_line::has_line_table (Dwarf_Off off) const
{
  return _m_line_tables.find (off) != _m_line_tables.end ();
}
