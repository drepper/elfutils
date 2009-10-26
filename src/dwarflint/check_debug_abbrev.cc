/* Pedantic checking of DWARF files
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "checks-low.hh"
#include "dwarfstrings.h"

#include <dwarf.h>
#include <sstream>
#include <cassert>
#include <algorithm>

/* Check that given form may in fact be valid in some CU.  */
static bool
check_abbrev_location_form (uint64_t form)
{
  switch (form)
    {
    case DW_FORM_indirect:

      /* loclistptr */
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_sec_offset: // DWARF 4

      /* block */
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
      return true;

    default:
      return false;
    };
}

void
abbrev_table_free (struct abbrev_table *abbr)
{
  for (struct abbrev_table *it = abbr; it != NULL; )
    {
      for (size_t i = 0; i < it->size; ++i)
	free (it->abbr[i].attribs);
      free (it->abbr);

      struct abbrev_table *temp = it;
      it = it->next;
      free (temp);
    }
}

struct abbrev *
abbrev_table_find_abbrev (struct abbrev_table *abbrevs, uint64_t abbrev_code)
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

bool
check_debug_abbrev::check_no_abbreviations () const
{
  bool ret = abbrevs.begin () == abbrevs.end ();
  if (ret)
    {
      where wh = WHERE (sec_abbrev, NULL);
      wr_error (&wh, ": no abbreviations.\n");
    }
  return ret;
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
}

check_debug_abbrev::check_debug_abbrev (dwarflint &lint)
  : _m_sec_abbr (lint.check (_m_sec_abbr))
{
  read_ctx ctx;
  read_ctx_init (&ctx, _m_sec_abbr->sect.data,
		 _m_sec_abbr->file.other_byte_order);

  struct abbrev_table *section = NULL;
  uint64_t first_attr_off = 0;
  struct where where = WHERE (sec_abbrev, NULL);
  where.addr1 = 0;

  while (true)
    {
      /* If we get EOF at this point, either the CU was improperly
	 terminated, or there were no data to begin with.  */
      if (read_ctx_eof (&ctx))
	{
	  if (!check_no_abbreviations ())
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
	  /* It still may have been empty.  */
	  check_no_abbreviations ();
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
	}

      struct abbrev *original = abbrev_table_find_abbrev (section, abbr_code);
      if (unlikely (original != NULL))
	{
	  std::stringstream ss;
	  ss << ": duplicate abbrev code " << abbr_code
	     << "; already defined at " << where_fmt (&original->where) << '.';
	  wr_error (&where, "%s\n", ss.str ().c_str ());
	}

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
	  std::stringstream ss;
	  ss << ": invalid abbrev tag 0x" << std::hex << abbr_tag << '.';
	  wr_error (&where, "%s\n", ss.str ().c_str ());
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
	  wr_error (&where,
		    ": invalid has_children value 0x%x.\n", cur->has_children);
	  throw check_base::failed ();
	}
      cur->has_children = has_children == DW_CHILDREN_yes;

      bool null_attrib;
      uint64_t sibling_attr = 0;
      bool low_pc = false;
      bool high_pc = false;
      bool ranges = false;
      do
	{
	  uint64_t attr_off = read_ctx_get_offset (&ctx);
	  uint64_t attrib_name, attrib_form;
	  if (first_attr_off == 0)
	    first_attr_off = attr_off;
	  /* Shift to match elfutils reporting.  */
	  where_reset_3 (&where, attr_off - first_attr_off);

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
		  std::stringstream ss;
		  ss << ": invalid name 0x" << std::hex << attrib_name << '.';
		  wr_error (&where, "%s\n", ss.str ().c_str ());
		  throw check_base::failed ();
		}

	      if (!attrib_form_valid (attrib_form))
		{
		  std::stringstream ss;
		  ss << ": invalid form 0x" << std::hex << attrib_form << '.';
		  wr_error (&where, "%s\n", ss.str ().c_str ());
		  throw check_base::failed ();
		}
	    }

	  REALLOC (cur, attribs);

	  struct abbrev_attrib *acur = cur->attribs + cur->size++;
	  WIPE (*acur);

	  /* We do structural checking of sibling attribute, so make
	     sure our assumptions in actual DIE-loading code are
	     right.  We expect at most one DW_AT_sibling attribute,
	     with form from reference class, but only CU-local, not
	     DW_FORM_ref_addr.  */
	  if (attrib_name == DW_AT_sibling)
	    {
	      if (sibling_attr != 0)
		{
		  std::stringstream ss;
		  ss << ": Another DW_AT_sibling attribute in one abbreviation. "
		     << "(First was 0x" << std::hex << sibling_attr << ".)";
		  wr_error (&where, "%s\n", ss.str ().c_str ());
		}
	      else
		{
		  assert (attr_off > 0);
		  sibling_attr = attr_off;

		  if (!cur->has_children)
		    wr_message (mc_die_rel | mc_acc_bloat | mc_impact_1,
				&where,
				": Excessive DW_AT_sibling attribute at childless abbrev.\n");
		}

	      switch (check_sibling_form (attrib_form))
		{
		case -1:
		  wr_message (mc_die_rel | mc_impact_2, &where,
			      ": DW_AT_sibling attribute with form DW_FORM_ref_addr.\n");
		  break;

		case -2:
		  wr_error (&where,
			    ": DW_AT_sibling attribute with non-reference form \"%s\".\n",
			    dwarf_form_string (attrib_form));
		};
	    }
	  /* Similar for DW_AT_location and friends.  */
	  else if (is_location_attrib (attrib_name))
	    {
	      if (!check_abbrev_location_form (attrib_form))
		wr_error (&where,
			  ": location attribute %s with invalid form \"%s\".\n",
			  dwarf_attr_string (attrib_name),
			  dwarf_form_string (attrib_form));
	    }
	  /* Similar for DW_AT_ranges.  */
	  else if (attrib_name == DW_AT_ranges
		   || attrib_name == DW_AT_stmt_list)
	    {
	      if (attrib_form != DW_FORM_data4
		  && attrib_form != DW_FORM_data8
		  && attrib_form != DW_FORM_sec_offset
		  && attrib_form != DW_FORM_indirect)
		wr_error (&where,
			  ": %s with invalid form \"%s\".\n",
			  dwarf_attr_string (attrib_name),
			  dwarf_form_string (attrib_form));
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
		wr_error (&where,
			  ": %s with invalid form \"%s\".\n",
			  dwarf_attr_string (attrib_name),
			  dwarf_form_string (attrib_form));

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
	wr_error (&where,
		  ": the abbrev has DW_AT_high_pc"
		  " without also having DW_AT_low_pc.\n");
      else if (high_pc && ranges)
	wr_error (&where,
		  ": the abbrev has DW_AT_high_pc & DW_AT_low_pc,"
		  " but also has DW_AT_ranges.\n");
    }

  abbrev_table *last = NULL;
  for (abbrev_map::iterator it = abbrevs.begin (); it != abbrevs.end (); ++it)
    {
      std::sort (it->second.abbr, it->second.abbr + it->second.size,
		 cmp_abbrev ());
      if (last != NULL)
	last->next = &it->second;
    }
}
