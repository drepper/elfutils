/* Pedantic checking of DWARF files
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "check_debug_info.hh"
#include "check_debug_abbrev.hh"
#include "pri.hh"
#include "tables.hh"
#include "sections.hh"
#include "checked_read.h"
#include "messages.h"
#include "tables.h"

#include <dwarf.h>
#include <sstream>
#include <cassert>
#include <algorithm>

checkdescriptor const *
check_debug_abbrev::descriptor ()
{
  static checkdescriptor cd
    (checkdescriptor::create ("check_debug_abbrev")
     .groups ("@low")
     .prereq <typeof (*_m_sec_abbr)> ()
     .prereq <typeof (*_m_cu_headers)> ()
     .description (
"Checks for low-level structure of .debug_abbrev.  In addition it\n"
"checks:\n"
" - that all abbreviation tables are non-empty\n"
" - that certain attribute forms match expectations (mainly those that\n"
"   we have to work with in subsequent check passes.  For example we\n"
"   check that DW_AT_low_pc has a form of DW_FORM_{,ref_}addr)\n"
" - that all CUs that share an abbrev table are of the same DWARF\n"
"   version\n"
" - that each abbrev table is used\n"
" - that abbrevs don't share abbrev codes\n"
" - that abbrev tags, attribute names and attribute forms are all known\n"
"   (note that this assumes that elfutils know about all tags used in\n"
"   practice.  Be sure to build against recent-enough version)\n"
" - that the value of has_children is either 0 or 1\n"
" - that DW_AT_sibling isn't formed as DW_FORM_ref_addr, and that it\n"
"   isn't present at childless abbrevs\n"
" - that attributes are not duplicated at abbrev\n"
" - that DW_AT_high_pc is never used without DW_AT_low_pc.  If both are\n"
"   used, that DW_AT_ranges isn't also used\n"
"This check generally requires CU headers to be readable, i.e. that the\n"
".debug_info section is roughly well-defined.  If that isn't the case,\n"
"many checks will still be done, operating under assumption that what\n"
"we see is the latest DWARF format.  This may render some checks\n"
"inaccurate.\n"));
  return &cd;
}

struct abbrev *
abbrev_table_find_abbrev (struct abbrev_table const *abbrevs,
			  uint64_t abbrev_code)
{
  size_t a = 0;
  size_t b = abbrevs->size;
  struct abbrev *ab = NULL;

  while (a < b)
    {
      size_t i = (a + b) / 2;
      ab = abbrevs->abbr + i;

      if (ab->code > abbrev_code)
	b = i;
      else if (ab->code < abbrev_code)
	a = i + 1;
      else
	return ab;
    }

  return NULL;
}

namespace
{
  struct cmp_abbrev
  {
    bool operator () (abbrev const &a, abbrev const &b) const
    {
      return a.code < b.code;
    }
  };

  void
  complain_invalid_form (where const &where, int name, int form,
			 std::string const &specification = "")
  {
    wr_error (where)
      << specification << (" "[specification.length () == 0])
      << pri::attr (name) << " with invalid form "
      << pri::form (form) << '.' << std::endl;
  }

  bool
  check_no_abbreviations (check_debug_abbrev::abbrev_map const &abbrevs)
  {
    bool ret = abbrevs.begin () == abbrevs.end ();
    if (ret)
      {
	where wh = WHERE (sec_abbrev, NULL);
	wr_error (&wh, ": no abbreviations.\n");
      }
    return ret;
  }

  check_debug_abbrev::abbrev_map
  load_debug_abbrev (sec &sect, elf_file &file,
		     read_cu_headers *cu_headers)
  {
    check_debug_abbrev::abbrev_map abbrevs;

    read_ctx ctx;
    read_ctx_init (&ctx, sect.data, file.other_byte_order);

    struct abbrev_table *section = NULL;
    uint64_t first_attr_off = 0;
    struct where where = WHERE (sec_abbrev, NULL);

    // Tolerate failure here.
    dwarf_version_h ver = NULL;
    if (cu_headers == NULL)
      {
	wr_error (where)
	  << "couldn't load CU headers; assuming CUs are of latest DWARF flavor."
	  << std::endl;
	ver = get_latest_dwarf_version ();
      }
    where.addr1 = 0;

    while (true)
      {
	/* If we get EOF at this point, either the CU was improperly
	   terminated, or there were no data to begin with.  */
	if (read_ctx_eof (&ctx))
	  {
	    if (!check_no_abbreviations (abbrevs))
	      wr_error (&where, ": missing zero to mark end-of-table.\n");
	    break;
	  }

	uint64_t abbr_off;
	uint64_t abbr_code;
	{
	  uint64_t prev_abbr_off = (uint64_t)-1;
	  uint64_t prev_abbr_code = (uint64_t)-1;
	  uint64_t zero_seq_off = (uint64_t)-1;

	  do
	    {
	      abbr_off = read_ctx_get_offset (&ctx);
	      where_reset_2 (&where, abbr_off);

	      /* Abbreviation code.  */
	      if (!checked_read_uleb128 (&ctx, &abbr_code, &where, "abbrev code"))
		throw check_base::failed ();

	      /* Note: we generally can't tell the difference between
		 empty table and (excessive) padding.  But NUL byte(s)
		 at the very beginning of section are almost certainly
		 the first case.  */
	      if (zero_seq_off == (uint64_t)-1
		  && abbr_code == 0
		  && (prev_abbr_code == 0
		      || abbrevs.empty ()))
		zero_seq_off = abbr_off;

	      if (abbr_code != 0)
		break;
	      else
		section = NULL;

	      prev_abbr_code = abbr_code;
	      prev_abbr_off = abbr_off;
	    }
	  while (!read_ctx_eof (&ctx)
		 /* On EOF, shift the offset so that beyond-EOF
		    end-position is printed for padding warning.
		    Necessary as our end position is exclusive.  */
		 || ((abbr_off += 1), false));

	  if (zero_seq_off != (uint64_t)-1)
	    {
	      struct where wh = WHERE (where.section, NULL);
	      wr_message_padding_0 (mc_abbrevs | mc_header,
				    &wh, zero_seq_off, abbr_off);
	    }
	}

	if (read_ctx_eof (&ctx))
	  {
	    /* It still could have been empty.  */
	    check_no_abbreviations (abbrevs);
	    break;
	  }

	/* OK, we got some genuine abbreviation.  See if we need to
	   allocate a new section.  */
	if (section == NULL)
	  {
	    abbrev_table t;
	    WIPE (t);
	    section = &abbrevs.insert (std::make_pair (abbr_off, t)).first->second;
	    section->offset = abbr_off;

	    where_reset_1 (&where, abbr_off);
	    where_reset_2 (&where, abbr_off);

	    // Find CU that uses this abbrev table, so that we know what
	    // version to validate against.
	    if (cu_headers != NULL)
	      {
		ver = NULL;
		cu_head const *other_head = NULL;
		for (std::vector <cu_head>::const_iterator it
		       = cu_headers->cu_headers.begin ();
		     it != cu_headers->cu_headers.end (); ++it)
		  if (it->abbrev_offset == abbr_off)
		    {
		      section->used = true;
		      dwarf_version_h nver = get_dwarf_version (it->version);
		      if (ver == NULL)
			ver = nver;
		      else if (nver != ver)
			{
			  wr_error (it->where)
			    << " and " << other_head->where << " both use "
			    << where << ", but each has a different version ("
			    << it->version << " vs. " << other_head->version
			    << ")." << std::endl;

			  // Arbitrarily pick newer version.
			  if (it->version > other_head->version)
			    ver = nver;
			}

		      other_head = &*it;
		    }

		if (ver == NULL)
		  {
		    // This is hard error, we can't validate abbrev
		    // table without knowing what version to use.
		    wr_error (where)
		      << "abbreviation table is never used." << std::endl;
		    ver = get_latest_dwarf_version ();
		  }
	      }
	    assert (ver != NULL);
	  }

	struct abbrev *original = abbrev_table_find_abbrev (section, abbr_code);
	if (unlikely (original != NULL))
	  wr_error (where)
	    << "duplicate abbrev code " << abbr_code
	    << "; already defined at " << original->where << '.' << std::endl;

	struct abbrev fake;
	struct abbrev *cur;
	/* Don't actually save this abbrev if it's duplicate.  */
	if (likely (original == NULL))
	  {
	    REALLOC (section, abbr);
	    cur = section->abbr + section->size++;
	  }
	else
	  cur = &fake;
	WIPE (*cur);

	cur->code = abbr_code;
	cur->where = where;

	/* Abbreviation tag.  */
	uint64_t abbr_tag;
	if (!checked_read_uleb128 (&ctx, &abbr_tag, &where, "abbrev tag"))
	  throw check_base::failed ();

	if (abbr_tag > DW_TAG_hi_user)
	  {
	    wr_error (where)
	      << "invalid abbrev tag " << pri::hex (abbr_tag)
	      << '.' << std::endl;
	    throw check_base::failed ();
	  }
	cur->tag = (typeof (cur->tag))abbr_tag;

	/* Abbreviation has_children.  */
	uint8_t has_children;
	if (!read_ctx_read_ubyte (&ctx, &has_children))
	  {
	    wr_error (&where, ": can't read abbrev has_children.\n");
	    throw check_base::failed ();
	  }

	if (has_children != DW_CHILDREN_no
	    && has_children != DW_CHILDREN_yes)
	  {
	    wr_error (where)
	      << "invalid has_children value " << pri::hex (cur->has_children)
	      << '.' << std::endl;
	    throw check_base::failed ();
	  }
	cur->has_children = has_children == DW_CHILDREN_yes;

	bool null_attrib;
	bool low_pc = false;
	bool high_pc = false;
	bool ranges = false;
	bool failed = false;
	std::map<unsigned, uint64_t> seen;

	do
	  {
	    uint64_t attr_off = read_ctx_get_offset (&ctx);
	    uint64_t attrib_name, attrib_form;
	    if (first_attr_off == 0)
	      first_attr_off = attr_off;

	    /* Shift to match elfutils reporting.  */
	    attr_off -= first_attr_off;
	    where_reset_3 (&where, attr_off);

	    /* Load attribute name and form.  */
	    if (!checked_read_uleb128 (&ctx, &attrib_name, &where,
				       "attribute name"))
	      throw check_base::failed ();

	    if (!checked_read_uleb128 (&ctx, &attrib_form, &where,
				       "attribute form"))
	      throw check_base::failed ();

	    null_attrib = attrib_name == 0 && attrib_form == 0;

	    /* Now if both are zero, this was the last attribute.  */
	    if (!null_attrib)
	      {
		/* Otherwise validate name and form.  */
		if (attrib_name > DW_AT_hi_user)
		  {
		    wr_error (where)
		      << "invalid name " << pri::hex (attrib_name)
		      << '.' << std::endl;
		    failed = true;
		    continue;
		  }

		if (!ver->form_allowed (attrib_form))
		  {
		    wr_error (where)
		      << "invalid form " << pri::hex (attrib_form)
		      << '.' << std::endl;
		    failed = true;
		    continue;
		  }

		std::pair<std::map<unsigned, uint64_t>::iterator, bool> inserted
		  = seen.insert (std::make_pair (attrib_name, attr_off));
		if (!inserted.second)
		  {
		    wr_error (where)
		      << "duplicate attribute " << pri::attr (attrib_name)
		      << " (first was at " << pri::hex (inserted.first->second)
		      << ")." << std::endl;
		    // I think we may allow such files for high-level
		    // consumption, so don't fail the check...
		    if (attrib_name == DW_AT_sibling)
		      // ... unless it's DW_AT_sibling.
		      failed = true;
		  }
	      }

	    REALLOC (cur, attribs);

	    struct abbrev_attrib *acur = cur->attribs + cur->size++;
	    WIPE (*acur);

	    /* We do structural checking of sibling attribute, so make
	       sure our assumptions in actual DIE-loading code are
	       right.  We expect form from reference class, but only
	       CU-local, not DW_FORM_ref_addr.  */
	    if (attrib_name == DW_AT_sibling)
	      {
		if (!cur->has_children)
		  wr_message (where,
			      cat (mc_die_rel, mc_acc_bloat, mc_impact_1))
		    << "excessive DW_AT_sibling attribute at childless abbrev."
		    << std::endl;

		switch (dwver_check_sibling_form (ver, attrib_form))
		  {
		  case -1:
		    wr_message (where, cat (mc_die_rel, mc_impact_2))
		      << "DW_AT_sibling attribute with form DW_FORM_ref_addr."
		      << std::endl;
		    break;

		  case -2:
		    wr_error (where)
		      << "DW_AT_sibling attribute with non-reference form "
		      << pri::form (attrib_form) << '.' << std::endl;
		  };
	      }

	    /* Similar for DW_AT_location and friends.  */
	    else if (is_location_attrib (attrib_name))
	      {
		if (!dwver_form_allowed (ver, attrib_name, attrib_form))
		  complain_invalid_form (where, attrib_name, attrib_form,
					 "location attribute");
	      }

	    /* Similar for DW_AT_ranges.  */
	    else if (attrib_name == DW_AT_ranges
		     || attrib_name == DW_AT_stmt_list)
	      {
		if (attrib_form != DW_FORM_data4
		    && attrib_form != DW_FORM_data8
		    && attrib_form != DW_FORM_sec_offset
		    && attrib_form != DW_FORM_indirect)
		  complain_invalid_form (where, attrib_name, attrib_form);
		if (attrib_name == DW_AT_ranges)
		  ranges = true;
	      }

	    /* Similar for DW_AT_{low,high}_pc, plus also make sure we
	       don't see high_pc without low_pc.  */
	    else if (attrib_name == DW_AT_low_pc
		     || attrib_name == DW_AT_high_pc)
	      {
		if (attrib_form != DW_FORM_addr
		    && attrib_form != DW_FORM_ref_addr)
		  complain_invalid_form (where, attrib_name, attrib_form);

		if (attrib_name == DW_AT_low_pc)
		  low_pc = true;
		else if (attrib_name == DW_AT_high_pc)
		  high_pc = true;
	      }

	    acur->name = attrib_name;
	    acur->form = attrib_form;
	    acur->where = where;
	  }
	while (!null_attrib);

	where_reset_2 (&where, where.addr2); // drop addr 3
	if (high_pc && !low_pc)
	  wr_error (where)
	    << "the abbrev has DW_AT_high_pc without also having DW_AT_low_pc."
	    << std::endl;
	else if (high_pc && ranges)
	  wr_error (where)
	    << "the abbrev has DW_AT_high_pc & DW_AT_low_pc, "
	    << "but also has DW_AT_ranges." << std::endl;

	if (failed)
	  throw check_base::failed ();
      }

    abbrev_table *last = NULL;
    for (check_debug_abbrev::abbrev_map::iterator it = abbrevs.begin ();
	 it != abbrevs.end (); ++it)
      {
	std::sort (it->second.abbr, it->second.abbr + it->second.size,
		   cmp_abbrev ());
	if (last != NULL)
	  last->next = &it->second;
	last = &it->second;
      }

    return abbrevs;
  }
}

check_debug_abbrev::check_debug_abbrev (checkstack &stack, dwarflint &lint)
  : _m_sec_abbr (lint.check (stack, _m_sec_abbr))
  , _m_cu_headers (lint.toplev_check (stack, _m_cu_headers))
  , abbrevs (load_debug_abbrev (_m_sec_abbr->sect,
				_m_sec_abbr->file,
				_m_cu_headers))
{
}

check_debug_abbrev::~check_debug_abbrev ()
{
  // xxx So using new[]/delete[] would be nicer (delete ignores
  // const-ness), but I'm not dipping into that right now.  Just cast
  // away the const, we're in the dtor so what the heck.
  abbrev_map &my_abbrevs = const_cast<abbrev_map &> (abbrevs);

  for (abbrev_map::iterator it = my_abbrevs.begin ();
       it != my_abbrevs.end (); ++it)
    {
      for (size_t i = 0; i < it->second.size; ++i)
	free (it->second.abbr[i].attribs);
      free (it->second.abbr);
    }
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
