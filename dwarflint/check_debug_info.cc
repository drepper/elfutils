/* Routines related to .debug_info.

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

#include <cassert>
#include <algorithm>
#include "../libdw/c++/dwarf"

#include "messages.hh"
#include "dwarf_version.hh"
#include "pri.hh"
#include "option.hh"
#include "sections.hh"
#include "checked_read.hh"
#include "check_debug_loc_range.hh"
#include "check_debug_abbrev.hh"
#include "check_debug_info.hh"
#include "check_debug_line.hh"
#include "check_debug_aranges.hh"

checkdescriptor const *
read_cu_headers::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("read_cu_headers")
     .hidden ());
  return &cd;
}

static void_option
  dump_die_offsets ("Dump DIE offsets to stderr as the tree is iterated.",
		    "dump-offsets");

checkdescriptor const *
check_debug_info::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_info")
     .groups ("@low")
     .schedule (false)
     .option (dump_die_offsets)
     .description (
"Checks for low-level structure of .debug_info.  In addition it "
"checks:\n"
" - for dangling reference to .debug_abbrev section\n"
" - that reported CU address sizes are consistent\n"
" - that rangeptr values are aligned to CU address size\n"
" - it is checked that DW_AT_low_pc and DW_AT_high_pc are relocated "
"consistently\n"
" - that DIE references are well formed (both intra-CU and inter-CU) "
"and that local reference isn't needlessly formed as global\n"
" - that .debug_string references are well formed and referred strings "
"are properly NUL-terminated\n"
" - that referenced abbreviations actually exist\n"
" - that DIEs with children have the DW_AT_sibling attribute and that "
"the sibling actually is at the address reported at that attribute\n"
" - that the DIE chain is terminated\n"
" - that the last sibling in chain has no DW_AT_sibling attribute\n"
" - that the DIE with children actually has children (i.e. that the "
"chain is not empty)\n"
" - for format constraints (such as that there are no 64-bit CUs inside "
"DWARF 2 file)\n"
" - in 32-bit CUs, that location attributes are not formed with "
"DW_FORM_data8\n"
" - all the attribute checks done by check_debug_abbrev are done here "
"for attributes with DW_FORM_indirect.  Indirect form is forbidden "
"to be again indirect\n"
" - that all abbreviations are used\n"
" - that relocations are valid.  In ET_REL files that certain fields "
"are relocated\n"
		   ));
  return &cd;
}

static reg<check_debug_info> reg_debug_info;

namespace
{
  bool
  check_category (enum message_category cat)
  {
    return message_accept (&warning_criteria, cat);
  }

  bool
  check_die_references (cu *cu, ref_record *die_refs)
  {
    bool retval = true;
    for (ref_record::const_iterator it = die_refs->begin ();
	 it != die_refs->end (); ++it)
      if (!cu->die_addrs.has_addr (it->addr))
	{
	  wr_error (it->who)
	    << "unresolved reference to " << pri::DIE (it->addr)
	    << '.' << std::endl;
	  retval = false;
	}
    return retval;
  }

  bool
  check_global_die_references (struct cu *cu_chain)
  {
    bool retval = true;
    for (struct cu *it = cu_chain; it != NULL; it = it->next)
      for (ref_record::const_iterator rt = it->die_refs.begin ();
	   rt != it->die_refs.end (); ++rt)
	{
	  struct cu *ref_cu = NULL;
	  for (struct cu *jt = cu_chain; jt != NULL; jt = jt->next)
	    if (jt->die_addrs.has_addr (rt->addr))
	      {
		ref_cu = jt;
		break;
	      }

	  if (ref_cu == NULL)
	    {
	      wr_error (rt->who)
		<< "unresolved (non-CU-local) reference to "
		<< pri::hex (rt->addr) << '.' << std::endl;
	      retval = false;
	    }
	  else if (ref_cu == it)
	    /* This is technically not a problem, so long as the
	       reference is valid, which it is.  But warn about this
	       anyway, perhaps local reference could be formed on
	       smaller number of bytes.  */
	    wr_message (rt->who, mc_impact_2 | mc_acc_suboptimal | mc_die_rel)
	      << "local reference to " << pri::DIE (rt->addr)
	      << " formed as global." << std::endl;
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
    uint64_t off_start, off_end;
    bool fail = false;

    std::vector <cu_head> ret;
    while (!read_ctx_eof (&ctx))
      {
	const unsigned char *cu_begin = ctx.ptr;
	uint64_t offset = read_ctx_get_offset (&ctx);
	cu_head head (offset);

	/* Reading CU head is a bit tricky, because we don't know if
	   we have run into (superfluous but allowed) zero padding
	   between CUs.  */

	if (!read_ctx_need_data (&ctx, 4)
	    && read_check_zero_padding (&ctx, &off_start, &off_end))
	  {
	    wr_message_padding_0 (mc_info | mc_header, head.where,
				  off_start, off_end);
	    break;
	  }

	/* CU length.  In DWARF 2, (uint32_t)-1 is simply a CU of that
	   length.  In DWARF 3+ that's an escape for 64bit length.
	   Unfortunately to read CU version, we have to get through
	   this field.  So we just assume that (uint32_t)-1 is an
	   escape in all cases.  */
	uint32_t size32;
	if (!read_ctx_read_4ubyte (&ctx, &size32))
	  {
	    wr_error (head.where) << "can't read CU length." << std::endl;
	    throw check_base::failed ();
	  }
	if (size32 == 0
	    && read_check_zero_padding (&ctx, &off_start, &off_end))
	  {
	    wr_message_padding_0 (mc_info | mc_header, head.where,
				  off_start, off_end);
	    break;
	  }

	Dwarf_Off cu_size;
	if (!read_size_extra (&ctx, size32, &cu_size,
			      &head.offset_size, head.where))
	  throw check_base::failed ();

	if (!read_ctx_need_data (&ctx, cu_size))
	  {
	    wr_error (head.where)
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
	if (dwarf_version::get (version) == NULL)
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

	/* Abbrev table offset.  */
	uint64_t ctx_offset = read_ctx_get_offset (&ctx);
	if (!read_ctx_read_offset (&ctx, head.offset_size == 8,
				   &head.abbrev_offset))
	  {
	    wr_error (head.where)
	      << "can't read abbrev table offset." << std::endl;
	    throw check_base::failed ();
	  }

	struct relocation *rel
	  = relocation_next (reloc, ctx_offset, head.where, skip_ok);
	if (rel != NULL)
	  {
	    relocate_one (file, reloc, rel, head.offset_size,
			  &head.abbrev_offset, head.where, sec_abbrev, NULL);
	    rel->invalid = true; // mark as invalid so it's skipped
				 // next time we pass by this
	  }
	else if (file->ehdr.e_type == ET_REL)
	  wr_message (head.where, mc_impact_2 | mc_info | mc_reloc)
	    << pri::lacks_relocation ("abbrev table offset") << std::endl;

	/* Address size.  */
	error_code err = read_address_size (&ctx, file->addr_64,
					    &head.address_size, head.where);
	if (err == err_fatal)
	  throw check_base::failed ();
	else if (err == err_nohl)
	  fail = true;

	head.head_size = ctx.ptr - cu_begin; // Length of the headers itself.
	head.total_size = cu_end - cu_begin; // Length including headers field.
	head.size = head.total_size - head.head_size;

	if (!read_ctx_skip (&ctx, head.size))
	  {
	    wr_error (head.where) << pri::not_enough ("next CU") << std::endl;
	    throw check_base::failed ();
	  }

	ret.push_back (head);
      }

    if (fail)
      throw check_base::failed ();

    return ret;
  }

  rel_target
  reloc_target (form const *form, attribute const *attribute)
  {
    switch (form->name ())
      {
      case DW_FORM_strp:
	return sec_str;

      case DW_FORM_addr:

	switch (attribute->name ())
	  {
	  case DW_AT_low_pc:
	  case DW_AT_high_pc:
	  case DW_AT_entry_pc:
	    return rel_target::rel_exec;

	  case DW_AT_const_value:
	    /* Appears in some kernel modules.  It's not allowed by the
	       standard, but leave that for high-level checks.  */
	    return rel_target::rel_address;
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
      case DW_FORM_sec_offset:

	switch (attribute->name ())
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

    std::cout << "XXX don't know how to handle form=" << *form
	      << ", at=" << *attribute << std::endl;

    return rel_target::rel_value;
  }

  struct value_check_cb_ctx
  {
    struct read_ctx *const ctx;
    die_locus const *where;
    struct cu *const cu;
    ref_record *local_die_refs;
    Elf_Data *strings;
    struct coverage *strings_coverage;
    struct coverage *pc_coverage;
    bool *need_rangesp;
    int *retval_p;
  };

  typedef void (*value_check_cb_t) (uint64_t addr,
				    struct value_check_cb_ctx const *ctx);

  /* Callback for local DIE references.  */
  void
  check_die_ref_local (uint64_t addr, struct value_check_cb_ctx const *ctx)
  {
    assert (ctx->ctx->end > ctx->ctx->begin);
    if (addr > (uint64_t)(ctx->ctx->end - ctx->ctx->begin))
      {
	wr_error (*ctx->where)
	  << "invalid reference outside the CU: " << pri::hex (addr)
	  << '.' << std::endl;
	return;
      }

    if (ctx->local_die_refs != NULL)
      /* Address holds a CU-local reference, so add CU offset
	 to turn it into section offset.  */
      ctx->local_die_refs->push_back (ref (addr + ctx->cu->head->offset,
					   *ctx->where));
  }

  /* Callback for global DIE references.  */
  void
  check_die_ref_global (uint64_t addr, struct value_check_cb_ctx const *ctx)
  {
    ctx->cu->die_refs.push_back (ref (addr, *ctx->where));
  }

  /* Callback for strp values.  */
  void
  check_strp (uint64_t addr, struct value_check_cb_ctx const *ctx)
  {
    if (ctx->strings == NULL)
      wr_error (*ctx->where)
	<< "strp attribute, but no .debug_str data." << std::endl;
    else if (addr >= ctx->strings->d_size)
      wr_error (*ctx->where)
	<< "invalid offset outside .debug_str: " << pri::hex (addr)
	<< '.' << std::endl;
    else
      {
	/* Record used part of .debug_str.  */
	const char *buf = static_cast <const char *> (ctx->strings->d_buf);
	const char *startp = buf + addr;
	const char *data_end = buf + ctx->strings->d_size;
	const char *strp = startp;
	while (strp < data_end && *strp != 0)
	  ++strp;
	if (strp == data_end)
	  {
	    wr_error (*ctx->where)
	      << "string at .debug_str: " << pri::hex (addr)
	      << " is not zero-terminated." << std::endl;
	    *ctx->retval_p = -2;
	  }

	if (ctx->strings_coverage != NULL)
	  ctx->strings_coverage->add (addr, strp - startp + 1);
      }
  }

  /* Callback for rangeptr values.  */
  void
  check_rangeptr (uint64_t value, struct value_check_cb_ctx const *ctx)
  {
    if ((value % ctx->cu->head->address_size) != 0)
      wr_message (*ctx->where, mc_ranges | mc_impact_2)
	<< "rangeptr value " << pri::hex (value)
	<< " not aligned to CU address size." << std::endl;
    *ctx->need_rangesp = true;
    ctx->cu->range_refs.push_back (ref (value, *ctx->where));
  }

  /* Callback for lineptr values.  */
  void
  check_lineptr (uint64_t value, struct value_check_cb_ctx const *ctx)
  {
    if (ctx->cu->stmt_list.addr != (uint64_t)-1)
      wr_error (*ctx->where)
	<< "DW_AT_stmt_list mentioned twice in a CU." << std::endl;
    ctx->cu->stmt_list = ref (value, *ctx->where);
  }

  /* Callback for locptr values.  */
  void
  check_locptr (uint64_t value, struct value_check_cb_ctx const *ctx)
  {
    ctx->cu->loc_refs.push_back (ref (value, *ctx->where));
  }

  void
  check_decl_file (uint64_t value, struct value_check_cb_ctx const *ctx)
  {
    ctx->cu->decl_file_refs.push_back (ref (value, *ctx->where));
  }

  /* The real sibling checking takes place down in read_die_chain.
     Here we just make sure that the value is non-zero.  That value is
     clearly invalid, and we use it to mark absent DW_AT_sibling.  */
  void
  check_sibling_non0 (uint64_t addr, struct value_check_cb_ctx const *ctx)
  {
    if (addr == 0)
      {
	wr_error (*ctx->where)
	  << "has a value of 0." << std::endl;
	// Don't let this up.
	*ctx->retval_p = -2;
      }
  }

  /*
    Returns:
    -2 in case of error that we have to note and return, but for now
       we can carry on
    -1 in case of error
    +0 in case of no error, but the chain only consisted of a
       terminating zero die.
    +1 in case some dies were actually loaded
  */
  int
  read_die_chain (dwarf_version const *ver,
		  elf_file const &file,
		  read_ctx *ctx,
		  cu *cu,
		  abbrev_table const *abbrevs,
		  Elf_Data *strings,
		  ref_record *local_die_refs,
		  coverage *strings_coverage,
		  relocation_data *reloc,
		  coverage *pc_coverage,
		  bool *need_rangesp,
		  unsigned level)
  {
    bool got_die = false;
    uint64_t sibling_addr = 0;
    uint64_t die_off, prev_die_off = 0;
    struct abbrev *abbrev = NULL;
    unsigned long die_count = 0;
    int retval = 0;

    struct value_check_cb_ctx cb_ctx = {
      ctx, NULL, cu,
      local_die_refs,
      strings, strings_coverage,
      pc_coverage,
      need_rangesp,
      &retval
    };

    while (!read_ctx_eof (ctx))
      {
	die_off = read_ctx_get_offset (ctx);
	/* Shift reported DIE offset by CU offset, to match the way
	   readelf reports DIEs.  */
	die_locus where (cu->head->offset + die_off);
	cb_ctx.where = &where;

	uint64_t abbr_code;

	if (!checked_read_uleb128 (ctx, &abbr_code, where, "abbrev code"))
	  return -1;

#define DEF_PREV_WHERE die_locus prev_where (cu->head->offset + prev_die_off)

	/* Check sibling value advertised last time through the loop.  */
	if (sibling_addr != 0)
	  {
	    if (abbr_code == 0)
	      {
		DEF_PREV_WHERE;
		wr_error (&prev_where,
			  ": is the last sibling in chain, "
			  "but has a DW_AT_sibling attribute.\n");
		/* dwarf_siblingof uses DW_AT_sibling to look for
		   sibling DIEs.  The value can't be right (there _is_
		   no sibling), so don't let this up.  */
		retval = -2;
	      }
	    else if (sibling_addr != die_off)
	      {
		DEF_PREV_WHERE;
		wr_error (prev_where)
		  << "this DIE claims that its sibling is "
		  << pri::hex (sibling_addr) << " but it's actually "
		  << pri::hex (die_off) << '.' << std::endl;
		retval = -2;
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
	    wr_message (prev_where, mc_die_rel | mc_acc_suboptimal | mc_impact_4)
	      << "This DIE had children, but no DW_AT_sibling attribute."
	      << std::endl;
	  }
#undef DEF_PREV_WHERE

	prev_die_off = die_off;

	/* The section ended.  */
	if (abbr_code == 0)
	  break;

	prev_die_off = die_off;
	got_die = true;

	/* Find the abbrev matching the code.  */
	abbrev = abbrevs->find_abbrev (abbr_code);
	if (abbrev == NULL)
	  {
	    wr_error (where)
	      << "abbrev section at " << pri::hex (abbrevs->offset)
	      << " doesn't contain code " << abbr_code << '.' << std::endl;
	    return -1;
	  }
	abbrev->used = true;

	if (dump_die_offsets)
	  std::cerr << "[" << level << "] "
		    << where << ": abbrev " << abbr_code
		    << "; DIE tag 0x" << std::hex << abbrev->tag << std::endl;

	// DWARF 4 Ch. 7.5: compilation unit header [is] followed by a
	// single DW_TAG_compile_unit or DW_TAG_partial_unit.
	bool is_cudie = level == 0
	  && (abbrev->tag == DW_TAG_compile_unit
	      || abbrev->tag == DW_TAG_partial_unit);
	if (level == 0)
	  {
	    if (++die_count > 1)
	      wr_error (where)
		<< "toplevel DIE chain contains more than one DIE."
		<< std::endl;
	    else if (!is_cudie)
	      {
		wr_error (cu->head->where)
		  << "toplevel DIE must be either compile_unit or partial_unit."
		  << std::endl;
		retval = -2;
	      }
	  }

	cu->die_addrs.add (cu->head->offset + die_off);

	uint64_t low_pc = (uint64_t)-1, high_pc = (uint64_t)-1;
	bool low_pc_relocated = false, high_pc_relocated = false;
	bool high_pc_relative = false;
	GElf_Sym low_pc_symbol_mem, *low_pc_symbol = &low_pc_symbol_mem;
	GElf_Sym high_pc_symbol_mem, *high_pc_symbol = &high_pc_symbol_mem;

	/* Attribute values.  */
	for (struct abbrev_attrib *it = abbrev->attribs;
	     it->name != 0 || it->form != 0; ++it)
	  {
	    where.set_attrib_name (it->name);
	    int form_name = it->form;

	    // In following, attribute may be NULL, but form never
	    // should.  We always need to know the form to be able to
	    // read .debug_info, so we fail in check_debug_abbrev if
	    // it's invalid or unknown.
	    attribute const *attribute = ver->get_attribute (it->name);
	    form const *form = ver->get_form (form_name);
	    if (attribute != NULL
		&& ver->form_class (form, attribute) == cl_indirect)
	      {
		uint64_t value;
		if (!read_sc_value (&value, form->width (cu->head),
				    ctx, where))
		  return -1;
		form_name = value;
		form = check_debug_abbrev::check_form
		  (ver, attribute, form_name, where, true);
		// N.B. check_form emits appropriate error messages.
		if (form == NULL)
		  return -1;
	      }
	    assert (form != NULL);

	    dw_class cls = attribute != NULL
	      ? ver->form_class (form, attribute)
	      : max_dw_class;
	    if (cls == cl_indirect)
	      {
		wr_error (&where, ": indirect form is again indirect.\n");
		return -1;
	      }

	    value_check_cb_t value_check_cb = NULL;

	    /* For checking lineptr, rangeptr, locptr.  */
	    bool check_someptr = false;
	    enum message_category extra_mc = mc_none;

	    uint64_t ctx_offset = read_ctx_get_offset (ctx) + cu->head->offset;
	    bool type_is_rel = file.ehdr.e_type == ET_REL;

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

	    /* Point to variable that you want to copy relocated value
	       to.  */
	    uint64_t *valuep = NULL;

	    /* Point to variable that you want set to `true' in case the
	       value was relocated.  */
	    bool *relocatedp = NULL;

	    /* Point to variable that you want set to symbol that the
	       relocation was made against.  */
	    GElf_Sym **symbolp = NULL;

	    static dw_class_set ref_classes
	      (cl_reference, cl_loclistptr, cl_lineptr, cl_macptr,
	       cl_rangelistptr);

	    if (form != NULL && cls != max_dw_class && ref_classes.test (cls))
	      {
		form_bitness_t bitness = form->bitness ();
		if ((bitness == fb_32 && cu->head->offset_size == 8)
		    || (bitness == fb_64 && cu->head->offset_size == 4))
		  wr_error (where)
		    << "reference attribute with form \""
		    << elfutils::dwarf::forms::name (form_name) << "\" in "
		    << (8 * cu->head->offset_size) << "-bit CU."
		    << std::endl;
	      }

	    /* Setup pointer checking.  */
	    switch (cls)
	      {
	      case cl_loclistptr:
		check_someptr = true;
		value_check_cb = check_locptr;
		extra_mc = mc_loc;
		break;

	      case cl_rangelistptr:
		check_someptr = true;
		value_check_cb = check_rangeptr;
		extra_mc = mc_ranges;
		break;

	      case cl_lineptr:
		check_someptr = true;
		value_check_cb = check_lineptr;
		extra_mc = mc_line;
		break;

	      default:
		;
	      }

	    /* Setup low_pc / high_pc checking.  */
	    switch (it->name)
	      {
	      case DW_AT_low_pc:
		relocatedp = &low_pc_relocated;
		symbolp = &low_pc_symbol;
		valuep = &low_pc;
		break;

	      case DW_AT_high_pc:
		relocatedp = &high_pc_relocated;
		symbolp = &high_pc_symbol;
		valuep = &high_pc;
		if (cls == cl_constant)
		  high_pc_relative = true;
		else if (cls != cl_address)
		  {
		    wr_error (&where, ": DW_AT_high_pc in unknown form.\n");
		    retval = -2;
		  }
		break;

	      case DW_AT_decl_file:
		value_check_cb = check_decl_file;
		break;
	      }

	    /* Setup per-form checking & relocation.  */
	    switch (form_name)
	      {
	      case DW_FORM_strp:
		value_check_cb = check_strp;
	      case DW_FORM_sec_offset:
		relocate = rel_require;
		break;

	      case DW_FORM_ref_addr:
		value_check_cb = check_die_ref_global;
	      case DW_FORM_addr:
		/* In non-rel files, neither addr, nor ref_addr /need/
		   a relocation.  */
		relocate = rel_nonzero;
		break;

	      case DW_FORM_ref_udata:
	      case DW_FORM_ref1:
	      case DW_FORM_ref2:
	      case DW_FORM_ref4:
	      case DW_FORM_ref8:
		value_check_cb = check_die_ref_local;
		break;

	      case DW_FORM_data4:
	      case DW_FORM_data8:
		if (check_someptr)
		  relocate = rel_require;
		break;
	      }

	    /* Attribute value.  */
	    uint64_t value;
	    read_ctx block;

	    storage_class_t storclass = form->storage_class ();
	    if (!read_generic_value (ctx, form->width (cu->head), storclass,
				     where, &value, &block))
	      {
		// Note that for fw_uleb and fw_sleb we report the
		// error the second time now.
		wr_error (where)
		  << "can't read value of attribute "
		  << elfutils::dwarf::attributes::name (it->name)
		  << '.' << std::endl;
		return -1;
	      }
	    if (storclass == sc_block)
	      {
		if (cls == cl_exprloc)
		  {
		    uint64_t expr_start
		      = cu->head->offset + read_ctx_get_offset (ctx) - value;
		    // xxx should we disallow relocation of length
		    // field?  See check_debug_loc_range::op_read_form
		    if (!check_location_expression
			(ver, file, &block, cu,
			 expr_start, reloc, value, where))
		      return -1;
		  }
		else
		  relocation_skip (reloc, read_ctx_get_offset (ctx),
				   where, skip_mismatched);
	      }

	    /* Relocate the value if appropriate.  */
	    struct relocation *rel;
	    if ((rel = relocation_next (reloc, ctx_offset,
					where, skip_mismatched)))
	      {
		if (relocate == rel_no)
		  wr_message (where, (mc_impact_4 | mc_die_other
				      | mc_reloc | extra_mc))
		    << "unexpected relocation of "
		    << elfutils::dwarf::forms::name (form_name)
		    << '.' << std::endl;

		if (attribute != NULL)
		  {
		    form_width_t width = form->width (cu->head);
		    relocate_one (&file, reloc, rel, width, &value, where,
				  reloc_target (form, attribute), symbolp);
		  }

		if (relocatedp != NULL)
		  *relocatedp = true;
	      }
	    else
	      {
		if (symbolp != NULL)
		  memset (*symbolp, 0, sizeof (**symbolp));
		if (type_is_rel
		    && (relocate == rel_require
			|| (relocate == rel_nonzero
			    && value != 0)))
		  wr_message (where, (mc_impact_2 | mc_die_other
				      | mc_reloc | extra_mc))
		    << pri::lacks_relocation
		        (elfutils::dwarf::forms::name (form_name))
		    << std::endl;
	      }

	    /* Dispatch value checking.  */
	    if (it->name == DW_AT_sibling)
	      {
		/* Full-blown DIE reference checking is too heavy-weight
		   and not practical (error messages wise) for checking
		   siblings.  */
		assert (value_check_cb == check_die_ref_local
			|| value_check_cb == check_die_ref_global);
		value_check_cb = check_sibling_non0;
		valuep = &sibling_addr;
	      }

	    if (value_check_cb != NULL)
	      value_check_cb (value, &cb_ctx);

	    /* Store the relocated value.  Note valuep may point to
	       low_pc or high_pc.  */
	    if (valuep != NULL)
	      *valuep = value;
	  }
	where.set_attrib_name (-1);

	if (high_pc != (uint64_t)-1 && low_pc != (uint64_t)-1
	    && high_pc_relative)
	  {
	    if (high_pc_relocated)
	      wr_message (where, mc_die_other | mc_impact_2 | mc_reloc)
		<< "DW_AT_high_pc is a constant (=relative), but is relocated."
		<< std::endl;
	    high_pc += low_pc;
	  }

	/* Check PC coverage.  We do that only for CU DIEs.  Any DIEs
	   lower in the tree (should) take subset of addresses taken
	   by the CU DIE.  */
	if (is_cudie && low_pc != (uint64_t)-1)
	  {
	    cu->low_pc = low_pc;

	    if (high_pc != (uint64_t)-1 && high_pc > low_pc)
	      pc_coverage->add (low_pc, high_pc - low_pc);
	  }

	if (high_pc != (uint64_t)-1 && low_pc != (uint64_t)-1)
	  {
	    if (!high_pc_relative && high_pc_relocated != low_pc_relocated)
	      wr_message (where, mc_die_other | mc_impact_2 | mc_reloc)
		<< "only one of DW_AT_low_pc and DW_AT_high_pc is relocated."
		<< std::endl;
	    else
	      {
		if (!high_pc_relative)
		  check_range_relocations (where, mc_die_other,
					   &file,
					   low_pc_symbol, high_pc_symbol,
					   "DW_AT_low_pc and DW_AT_high_pc");
		/* If there is no coverage, these attributes should
		   not ever be there.  */
		if (low_pc > high_pc || low_pc == high_pc)
		  wr_message (where, mc_die_other | mc_impact_3)
		    << "DW_AT_low_pc value not below DW_AT_high_pc."
		    << std::endl;
	      }
	  }

	if (abbrev->has_children)
	  {
	    int st = read_die_chain (ver, file, ctx, cu, abbrevs, strings,
				     local_die_refs,
				     strings_coverage, reloc,
				     pc_coverage, need_rangesp, level + 1);
	    if (st == -1)
	      return -1;
	    else if (st == -2)
	      retval = -2;
	    else if (st == 0)
	      wr_message (mc_impact_3 | mc_acc_suboptimal | mc_die_rel,
			  &where,
			  ": abbrev has_children, but the chain was empty.\n");
	  }

	if (read_ctx_eof (ctx))
	  {
	    if (level > 0)
	      // DWARF 4 Ch. 2.3: A chain of sibling entries is
	      // terminated by a null entry.  N.B. the CU DIE is a
	      // singleton, not part of a DIE chain.
	      wr_error (where)
		<< "DIE chain not terminated with null entry." << std::endl;
	    break;
	  }
      }

    if (sibling_addr != 0)
      wr_error (die_locus (cu->head->offset + prev_die_off))
	<< "this DIE should have had its sibling at " << pri::hex (sibling_addr)
	<< ", but the DIE chain ended." << std::endl;

    if (retval != 0)
      return retval;
    else
      return got_die ? 1 : 0;
  }
}

read_cu_headers::read_cu_headers (checkstack &stack, dwarflint &lint)
  : _m_sec_info (lint.check (stack, _m_sec_info))
  , cu_headers (read_info_headers (&_m_sec_info->file,
				   &_m_sec_info->sect,
				   _m_sec_info->reldata ()))
{
}

bool
check_debug_info::check_cu_structural (struct read_ctx *ctx,
				       struct cu *const cu,
				       Elf_Data *strings,
				       struct coverage *strings_coverage,
				       struct relocation_data *reloc)
{
  check_debug_abbrev::abbrev_map const &abbrev_tables = _m_abbrevs->abbrevs;

  if (dump_die_offsets)
    fprintf (stderr, "%s: CU starts\n", cu->head->where.format ().c_str ());
  bool retval = true;

  dwarf_version const *ver = dwarf_version::get (cu->head->version);
  assert (ver != NULL);

  /* Look up Abbrev table for this CU.  */
  check_debug_abbrev::abbrev_map::const_iterator abbrev_it
    = abbrev_tables.find (cu->head->abbrev_offset);
  if (abbrev_it == abbrev_tables.end ())
    {
      wr_error (cu->head->where)
	<< "couldn't find abbrev section with offset "
	<< pri::addr (cu->head->abbrev_offset) << '.' << std::endl;
      return false;
    }
  struct abbrev_table const &abbrevs = abbrev_it->second;

  /* Read DIEs.  */
  ref_record local_die_refs;

  cu->cudie_offset = read_ctx_get_offset (ctx) + cu->head->offset;
  int st = read_die_chain (ver, _m_file, ctx, cu, &abbrevs, strings,
			   &local_die_refs, strings_coverage,
			   (reloc != NULL && reloc->size > 0) ? reloc : NULL,
			   &_m_cov, &_m_need_ranges, 0);
  if (st < 0)
    {
      _m_abbr_skip.push_back (abbrevs.offset);
      retval = false;
    }
  else if (st == 0)
    wr_error (cu->head->where)
      << "CU contains no DIEs." << std::endl;
  else if (!check_die_references (cu, &local_die_refs))
    retval = false;

  return retval;
}

check_debug_info::check_debug_info (checkstack &stack, dwarflint &lint)
  : _m_sec_info (lint.check (stack, _m_sec_info))
  , _m_sec_str (lint.check (stack, _m_sec_str))
  , _m_file (_m_sec_info->file)
  , _m_abbrevs (lint.check (stack, _m_abbrevs))
  , _m_cu_headers (lint.check (stack, _m_cu_headers))
  , _m_need_ranges (false)
{
  std::vector <cu_head> const &cu_headers = _m_cu_headers->cu_headers;
  sec &sec = _m_sec_info->sect;
  Elf_Data *const strings = _m_sec_str->sect.data;

  ref_record die_refs;

  bool success = true;

  coverage *strings_coverage =
    (strings != NULL && check_category (mc_strings))
    ? new coverage () : NULL;

  struct relocation_data *reloc = sec.rel.size > 0 ? &sec.rel : NULL;
  if (reloc != NULL)
    relocation_reset (reloc);

  struct read_ctx ctx;
  read_ctx_init (&ctx, sec.data, _m_file.other_byte_order);
  for (std::vector <cu_head>::const_iterator it = cu_headers.begin ();
       it != cu_headers.end (); ++it)
    {
      cu_head const &head = *it;
      cu_locus where = head.where;
      {
	cu cur;
	memset (&cur, 0, sizeof (cur));
	cur.head = &head;
	cur.low_pc = cur.stmt_list.addr = (uint64_t)-1;
	cur.next = (cu *)(uintptr_t)0xdead;
	cus.push_back (cur);
      }
      cu &cur = cus.back ();

      assert (read_ctx_need_data (&ctx, head.total_size));

      // Make CU context begin just before the CU length, so that
      // DIE offsets are computed correctly.
      struct read_ctx cu_ctx;
      const unsigned char *cu_end = ctx.ptr + head.total_size;
      read_ctx_init_sub (&cu_ctx, &ctx, ctx.ptr, cu_end);
      cu_ctx.ptr += head.head_size;

      if (!check_cu_structural (&cu_ctx, &cur,
				strings, strings_coverage, reloc))
	{
	  success = false;
	  break;
	}

      if (cu_ctx.ptr != cu_ctx.end)
	{
	  uint64_t off_start, off_end;
	  if (read_check_zero_padding (&cu_ctx, &off_start, &off_end))
	    wr_message_padding_0 (mc_info, where, off_start, off_end);
	  else
	    {
	      // Garbage coordinates:
	      uint64_t start = read_ctx_get_offset (&ctx) + off_start;
	      uint64_t end = read_ctx_get_offset (&ctx) + head.total_size;
	      wr_message_padding_n0 (mc_info, where, start, end);
	    }
	}

      int i = read_ctx_skip (&ctx, head.total_size);
      assert (i);
    }

  if (success)
    {
      section_locus wh (sec_info);
      if (ctx.ptr != ctx.end)
	/* Did we read up everything?  */
	wr_message (mc_die_other | mc_impact_4, &wh,
		    ": CU lengths don't exactly match Elf_Data contents.");
      else
	/* Did we consume all the relocations?  */
	relocation_skip_rest (&sec.rel, wh);

      /* If we managed to read up everything, now do abbrev usage
	 analysis.  */
      for (check_debug_abbrev::abbrev_map::const_iterator it
	     = _m_abbrevs->abbrevs.begin ();
	   it != _m_abbrevs->abbrevs.end (); ++it)
	if (it->second.used
	    && std::find (_m_abbr_skip.begin (), _m_abbr_skip.end (),
			  it->first) == _m_abbr_skip.end ())
	  for (size_t i = 0; i < it->second.size; ++i)
	    if (!it->second.abbr[i].used)
	      wr_message (it->second.abbr[i].where,
			  mc_impact_3 | mc_acc_bloat | mc_abbrevs)
		<< "abbreviation is never used." << std::endl;
    }

  // re-link CUs so that they form a chain again.  This is to
  // interface with legacy code.
  {
    cu *last = NULL;
    for (std::vector<cu>::iterator it = cus.begin ();
	 it != cus.end (); ++it)
      {
	cu *cur = &*it;
	if (last != NULL)
	  last->next = cur;
	last = cur;
      }
    if (last != NULL)
      last->next = NULL;
  }

  /* We used to check that all CUs have the same address size.  Now
     that we validate address_size of each CU against the ELF header,
     that's not necessary anymore.  */

  check_global_die_references (!cus.empty () ? &cus.front () : NULL);

  if (strings_coverage != NULL)
    {
      if (success)
	{
	  struct hole_info info = {sec_str, mc_strings, strings->d_buf, 0};
	  strings_coverage->find_holes (0, strings->d_size, found_hole, &info);
	}
      delete strings_coverage;
    }

  // If we were unsuccessful, fail now.
  if (!success)
    throw check_base::failed ();

  if (cus.size () > 0)
    assert (cus.back ().next == NULL);
}

check_debug_info::~check_debug_info ()
{
}

cu *
check_debug_info::find_cu (::Dwarf_Off offset)
{
  for (std::vector<cu>::iterator it = cus.begin ();
       it != cus.end (); ++it)
    if (it->head->offset == offset)
      return &*it;

  return NULL;
}

checkdescriptor const *
check_debug_info_refs::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_info_refs")
     .groups ("@low")
     .schedule (false)
     .description (
"This pass checks:\n"
" - for outstanding unresolved references from .debug_info to .debug_line\n"
" - that each CU has an associated aranges entry (that even if there is "
"no .debug_aranges to begin with).\n"));
  return &cd;
}

static reg<check_debug_info_refs> reg_debug_info_refs;

check_debug_info_refs::check_debug_info_refs (checkstack &stack,
					      dwarflint &lint)
  : _m_info (lint.check (stack, _m_info))
  , _m_line (lint.toplev_check (stack, _m_line))
  , _m_aranges (lint.toplev_check (stack, _m_aranges))
{
  // XXX if .debug_line is present and broken, we don't want to report
  // every unsatisfied reference.  If .debug_line is absent and
  // references are present, we want to diagnose that in one line.  If
  // .debug_line is present and valid, then we want to check each
  // reference separately.
  for (std::vector<cu>::iterator it = _m_info->cus.begin ();
       it != _m_info->cus.end (); ++it)
    {
      if (it->stmt_list.addr == (uint64_t)-1)
	for (ref_record::const_iterator jt = it->decl_file_refs.begin ();
	     jt != it->decl_file_refs.end (); ++jt)
	  wr_error (jt->who)
	    << "references .debug_line table, but CU DIE lacks DW_AT_stmt_list."
	    << std::endl;
      else if (_m_line == NULL
	       || !_m_line->has_line_table (it->stmt_list.addr))
	wr_error (it->stmt_list.who)
	  << "unresolved reference to .debug_line table "
	  << pri::hex (it->stmt_list.addr) << '.' << std::endl;

      if (_m_aranges != NULL && !it->has_arange)
	wr_message (it->head->where,
		    mc_impact_3 | mc_acc_suboptimal | mc_aranges | mc_info)
	  << "no aranges table is associated with this CU." << std::endl;
    }
}
