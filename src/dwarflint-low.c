/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009 Red Hat, Inc.
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

#include <assert.h>
#include <error.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
#include <unistd.h>

#include "../libdw/dwarf.h"
#include "../libdw/known-dwarf.h"
#include "../libebl/libebl.h"
#include "dwarfstrings.h"
#include "dwarflint-low.h"
#include "dwarflint-readctx.h"
#include "dwarflint-config.h"
#include "dwarf-opcodes.h"

/* True if coverage analysis of .debug_ranges vs. ELF sections should
   be done.  */
static const bool do_range_coverage = false;

static bool
check_category (enum message_category cat)
{
  return message_accept (&warning_criteria, cat);
}

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64
#define PRI_NOT_ENOUGH ": not enough data for %s.\n"
#define PRI_LACK_RELOCATION ": %s seems to lack a relocation.\n"


/* Functions and data structures related to raw (i.e. unassisted by
   libdw) Dwarf abbreviation handling.  */

struct abbrev
{
  uint64_t code;
  struct where where;

  /* Attributes.  */
  struct abbrev_attrib
  {
    struct where where;
    uint16_t name;
    uint8_t form;
  } *attribs;
  size_t size;
  size_t alloc;

  /* While ULEB128 can hold numbers > 32bit, these are not legal
     values of many enum types.  So just use as large type as
     necessary to cover valid values.  */
  uint16_t tag;
  bool has_children;

  /* Whether some DIE uses this abbrev.  */
  bool used;
};

struct abbrev_table
{
  struct abbrev_table *next;
  struct abbrev *abbr;
  uint64_t offset;
  size_t size;
  size_t alloc;
  bool used;		/* There are CUs using this table.  */
  bool skip_check;	/* There were errors during loading one of the
			   CUs that use this table.  Check for unused
			   abbrevs should be skipped.  */
};

/* Functions and data structures for address record handling.  We use
   that to check that all DIE references actually point to an existing
   die, not somewhere mid-DIE, where it just happens to be
   interpretable as a DIE.  */

struct addr_record
{
  size_t size;
  size_t alloc;
  uint64_t *addrs;
};

static size_t addr_record_find_addr (struct addr_record *ar, uint64_t addr);
static bool addr_record_has_addr (struct addr_record *ar, uint64_t addr);
static void addr_record_add (struct addr_record *ar, uint64_t addr);
static void addr_record_free (struct addr_record *ar);


/* Functions and data structures for reference handling.  Just like
   the above, we use this to check validity of DIE references.  Unlike
   the above, this is not stored as sorted set, but simply as an array
   of records, because duplicates are unlikely.  */

struct ref
{
  uint64_t addr; // Referree address
  struct where who;  // Referrer
};

struct ref_record
{
  size_t size;
  size_t alloc;
  struct ref *refs;
};

static void ref_record_add (struct ref_record *rr, uint64_t addr, struct where *referrer);
static void ref_record_free (struct ref_record *rr);


/* Functions and data structures for CU handling.  */

struct cu
{
  struct cu *next;
  uint64_t offset;
  uint64_t cudie_offset;
  uint64_t length;
  uint64_t low_pc;              // DW_AT_low_pc value of CU DIE, -1 if not present.
  struct addr_record die_addrs; // Addresses where DIEs begin in this CU.
  struct ref_record die_refs;   // DIE references into other CUs from this CU.
  struct ref_record loc_refs;   // references into .debug_loc from this CU.
  struct ref_record range_refs; // references into .debug_ranges from this CU.
  struct ref_record line_refs;	// references into .debug_line from this CU.
  struct where where;           // Where was this section defined.
  int address_size;             // Address size in bytes on the target machine.
  int offset_size;		// Offset size in this CU.
  int version;			// CU version
  bool has_arange;              // Whether we saw arange section pointing to this CU.
  bool has_pubnames;            // Likewise for pubnames.
  bool has_pubtypes;            // Likewise for pubtypes.
};

static struct cu *cu_find_cu (struct cu *cu_chain, uint64_t offset);

static bool check_location_expression (struct elf_file *file,
				       struct read_ctx *ctx,
				       struct cu *cu,
				       uint64_t init_off,
				       struct relocation_data *reloc,
				       size_t length,
				       struct where *wh);

bool
address_aligned (uint64_t addr, uint64_t align)
{
  return align < 2 || (addr % align == 0);
}

bool
necessary_alignment (uint64_t start, uint64_t length, uint64_t align)
{
  return address_aligned (start + length, align) && length < align;
}

static bool
checked_read_uleb128 (struct read_ctx *ctx, uint64_t *ret,
		      struct where *where, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_uleb128 (ctx, ret);
  if (st < 0)
    wr_error (where, ": can't read %s.\n", what);
  else if (st > 0)
    {
      char buf[19]; // 16 hexa digits, "0x", terminating zero
      sprintf (buf, "%#" PRIx64, *ret);
      wr_format_leb128_message (where, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

static bool
checked_read_sleb128 (struct read_ctx *ctx, int64_t *ret,
		      struct where *where, const char *what)
{
  const unsigned char *ptr = ctx->ptr;
  int st = read_ctx_read_sleb128 (ctx, ret);
  if (st < 0)
    wr_error (where, ": can't read %s.\n", what);
  else if (st > 0)
    {
      char buf[20]; // sign, "0x", 16 hexa digits, terminating zero
      int64_t val = *ret;
      sprintf (buf, "%s%#" PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
      wr_format_leb128_message (where, what, buf, ptr, ctx->ptr);
    }
  return st >= 0;
}

/* The value passed back in uint64_t VALUEP may actually be
   type-casted int64_t.  WHAT and WHERE describe error message and
   context for LEB128 loading.  */
static bool
read_ctx_read_form (struct read_ctx *ctx, struct cu *cu, uint8_t form,
		    uint64_t *valuep, struct where *where, const char *what)
{
  switch (form)
    {
    case DW_FORM_addr:
      return read_ctx_read_offset (ctx, cu->address_size == 8, valuep);
    case DW_FORM_udata:
      return checked_read_uleb128 (ctx, valuep, where, what);
    case DW_FORM_sdata:
      return checked_read_sleb128 (ctx, (int64_t *)valuep, where, what);
    case DW_FORM_data1:
      {
	uint8_t v;
	if (!read_ctx_read_ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data2:
      {
	uint16_t v;
	if (!read_ctx_read_2ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data4:
      {
	uint32_t v;
	if (!read_ctx_read_4ubyte (ctx, &v))
	  return false;
	if (valuep != NULL)
	  *valuep = v;
	return true;
      }
    case DW_FORM_data8:
      return read_ctx_read_8ubyte (ctx, valuep);
    };

  return false;
}

static bool
attrib_form_valid (uint64_t form)
{
  return form > 0 && form <= DW_FORM_ref_sig8;
}

static int
check_sibling_form (uint64_t form)
{
  switch (form)
    {
    case DW_FORM_indirect:
      /* Tolerate this in abbrev loading, even during the DIE loading.
	 We check that dereferenced indirect form yields valid form.  */
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      return 0;

    case DW_FORM_ref_addr:
      return -1;

    default:
      return -2;
    };
}

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

static bool
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

struct abbrev_table *
abbrev_table_load (struct read_ctx *ctx)
{
  struct abbrev_table *section_chain = NULL;
  struct abbrev_table *section = NULL;
  uint64_t first_attr_off = 0;
  struct where where = WHERE (sec_abbrev, NULL);
  where.addr1 = 0;

  while (true)
    {
      inline bool check_no_abbreviations ()
      {
	bool ret = section_chain == NULL;
	if (ret)
	  wr_error (&WHERE (sec_abbrev, NULL), ": no abbreviations.\n");
	return ret;
      }

      /* If we get EOF at this point, either the CU was improperly
	 terminated, or there were no data to begin with.  */
      if (read_ctx_eof (ctx))
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
	    abbr_off = read_ctx_get_offset (ctx);
	    where_reset_2 (&where, abbr_off);

	    /* Abbreviation code.  */
	    if (!checked_read_uleb128 (ctx, &abbr_code, &where, "abbrev code"))
	      goto free_and_out;

	    /* Note: we generally can't tell the difference between
    	       empty table and (excessive) padding.  But NUL byte(s)
    	       at the very beginning of section are almost certainly
    	       the first case.  */
	    if (zero_seq_off == (uint64_t)-1
		&& abbr_code == 0
		&& (prev_abbr_code == 0
		    || section_chain == NULL))
	      zero_seq_off = abbr_off;

	    if (abbr_code != 0)
	      break;
	    else
	      section = NULL;

	    prev_abbr_code = abbr_code;
	    prev_abbr_off = abbr_off;
	  }
	while (!read_ctx_eof (ctx)
	       /* On EOF, shift the offset so that beyond-EOF
		  end-position is printed for padding warning.
		  Necessary as our end position is exclusive.  */
	       || ((abbr_off += 1), false));

	if (zero_seq_off != (uint64_t)-1)
	  wr_message_padding_0 (mc_abbrevs | mc_header,
				&WHERE (where.section, NULL),
				zero_seq_off, abbr_off);
      }

      if (read_ctx_eof (ctx))
	{
	  /* It still may have been empty.  */
	  check_no_abbreviations ();
	  break;
	}

      /* OK, we got some genuine abbreviation.  See if we need to
	 allocate a new section.  */
      if (section == NULL)
	{
	  section = xcalloc (1, sizeof (*section));
	  section->offset = abbr_off;
	  section->next = section_chain;
	  section_chain = section;

	  where_reset_1 (&where, abbr_off);
	  where_reset_2 (&where, abbr_off);
	}

      struct abbrev *original = abbrev_table_find_abbrev (section, abbr_code);
      if (unlikely (original != NULL))
	{
	  char *site1 = strdup (where_fmt (&original->where, NULL));
	  wr_error (&where, ": duplicate abbrev code %" PRId64
		    "; already defined at %s.\n", abbr_code, site1);
	  free (site1);
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
      if (!checked_read_uleb128 (ctx, &abbr_tag, &where, "abbrev tag"))
	goto free_and_out;

      if (abbr_tag > DW_TAG_hi_user)
	{
	  wr_error (&where, ": invalid abbrev tag 0x%" PRIx64 ".\n", abbr_tag);
	  goto free_and_out;
	}
      cur->tag = (typeof (cur->tag))abbr_tag;

      /* Abbreviation has_children.  */
      uint8_t has_children;
      if (!read_ctx_read_ubyte (ctx, &has_children))
	{
	  wr_error (&where, ": can't read abbrev has_children.\n");
	  goto free_and_out;
	}

      if (has_children != DW_CHILDREN_no
	  && has_children != DW_CHILDREN_yes)
	{
	  wr_error (&where,
		    ": invalid has_children value 0x%x.\n", cur->has_children);
	  goto free_and_out;
	}
      cur->has_children = has_children == DW_CHILDREN_yes;

      bool null_attrib;
      uint64_t sibling_attr = 0;
      bool low_pc = false;
      bool high_pc = false;
      bool ranges = false;
      do
	{
	  uint64_t attr_off = read_ctx_get_offset (ctx);
	  uint64_t attrib_name, attrib_form;
	  if (first_attr_off == 0)
	    first_attr_off = attr_off;
	  /* Shift to match elfutils reporting.  */
	  where_reset_3 (&where, attr_off - first_attr_off);

	  /* Load attribute name and form.  */
	  if (!checked_read_uleb128 (ctx, &attrib_name, &where,
				     "attribute name"))
	    goto free_and_out;

	  if (!checked_read_uleb128 (ctx, &attrib_form, &where,
				     "attribute form"))
	    goto free_and_out;

	  null_attrib = attrib_name == 0 && attrib_form == 0;

	  /* Now if both are zero, this was the last attribute.  */
	  if (!null_attrib)
	    {
	      /* Otherwise validate name and form.  */
	      if (attrib_name > DW_AT_hi_user)
		{
		  wr_error (&where,
			    ": invalid name 0x%" PRIx64 ".\n", attrib_name);
		  goto free_and_out;
		}

	      if (!attrib_form_valid (attrib_form))
		{
		  wr_error (&where,
			    ": invalid form 0x%" PRIx64 ".\n", attrib_form);
		  goto free_and_out;
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
		wr_error (&where,
			  ": Another DW_AT_sibling attribute in one abbreviation. "
			  "(First was 0x%" PRIx64 ".)\n", sibling_attr);
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
		  ": the abbrev has DW_AT_high_pc without also having DW_AT_low_pc.\n");
      else if (high_pc && ranges)
	wr_error (&where,
		  ": the abbrev has DW_AT_high_pc & DW_AT_low_pc, but also has DW_AT_ranges.\n");
    }

  for (section = section_chain; section != NULL; section = section->next)
    {
      int cmp_abbrs (const void *a, const void *b)
      {
	struct abbrev *aa = (struct abbrev *)a;
	struct abbrev *bb = (struct abbrev *)b;
	return aa->code - bb->code;
      }

      /* The array is most likely already sorted in the file, but just
	 to be sure...  */
      qsort (section->abbr, section->size, sizeof (*section->abbr), cmp_abbrs);
    }

  return section_chain;

 free_and_out:
  abbrev_table_free (section_chain);
  return NULL;
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

static size_t
addr_record_find_addr (struct addr_record *ar, uint64_t addr)
{
  size_t a = 0;
  size_t b = ar->size;

  while (a < b)
    {
      size_t i = (a + b) / 2;
      uint64_t v = ar->addrs[i];

      if (v > addr)
	b = i;
      else if (v < addr)
	a = i + 1;
      else
	return i;
    }

  return a;
}

static bool
addr_record_has_addr (struct addr_record *ar, uint64_t addr)
{
  if (ar->size == 0
      || addr < ar->addrs[0]
      || addr > ar->addrs[ar->size - 1])
    return false;

  size_t a = addr_record_find_addr (ar, addr);
  return a < ar->size && ar->addrs[a] == addr;
}

static void
addr_record_add (struct addr_record *ar, uint64_t addr)
{
  size_t a = addr_record_find_addr (ar, addr);
  if (a >= ar->size || ar->addrs[a] != addr)
    {
      REALLOC (ar, addrs);
      size_t len = ar->size - a;
      memmove (ar->addrs + a + 1, ar->addrs + a, len * sizeof (*ar->addrs));

      ar->addrs[a] = addr;
      ar->size++;
    }
}

static void
addr_record_free (struct addr_record *ar)
{
  if (ar != NULL)
    free (ar->addrs);
}


static void
ref_record_add (struct ref_record *rr, uint64_t addr, struct where *referrer)
{
  REALLOC (rr, refs);
  struct ref *ref = rr->refs + rr->size++;
  ref->addr = addr;
  ref->who = *referrer;
}

static void
ref_record_free (struct ref_record *rr)
{
  if (rr != NULL)
    free (rr->refs);
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
      if (!(info->align != 0 && info->align != 1
	    && (end % info->align == 0) && (start % 4 != 0)
	    && (length < info->align)))
	wr_message_padding_0 (info->category, &WHERE (info->section, NULL),
			      start, end);
    }
  else
    /* XXX: This actually lies when the unreferenced portion is
       composed of sequences of zeroes and non-zeroes.  */
    wr_message_padding_n0 (info->category, &WHERE (info->section, NULL),
			   start, end);

  return true;
}

/* begin is inclusive, end is exclusive. */
bool
coverage_map_found_hole (uint64_t begin, uint64_t end,
			 struct section_coverage *sco, void *user)
{
  struct coverage_map_hole_info *info = (struct coverage_map_hole_info *)user;

  struct where where = WHERE (info->info.section, NULL);
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
  wr_message (info->info.category | mc_acc_suboptimal | mc_impact_4, &where,
	      ": addresses %s of section %s are not covered.\n",
	      range_fmt (buf, sizeof buf, begin + base, end + base), scnname);
  return true;
}


void
section_coverage_init (struct section_coverage *sco,
		       struct sec *sec, bool warn)
{
  assert (sco != NULL);
  assert (sec != NULL);

  sco->sec = sec;
  WIPE (sco->cov);
  sco->hit = false;
  sco->warn = warn;
}

bool
coverage_map_init (struct coverage_map *coverage_map,
		   struct elf_file *elf,
		   Elf64_Xword mask,
		   Elf64_Xword warn_mask,
		   bool allow_overlap)
{
  assert (coverage_map != NULL);
  assert (elf != NULL);

  WIPE (*coverage_map);
  coverage_map->elf = elf;
  coverage_map->allow_overlap = allow_overlap;

  for (size_t i = 1; i < elf->size; ++i)
    {
      struct sec *sec = elf->sec + i;

      bool normal = (sec->shdr.sh_flags & mask) == mask;
      bool warn = (sec->shdr.sh_flags & warn_mask) == warn_mask;
      if (normal || warn)
	{
	  REALLOC (coverage_map, scos);
	  section_coverage_init
	    (coverage_map->scos + coverage_map->size++, sec, !normal);
	}
    }

  return true;
}

void
coverage_map_add (struct coverage_map *coverage_map,
		  uint64_t address,
		  uint64_t length,
		  struct where *where,
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
  struct coverage range_cov;
  WIPE (range_cov);

  for (size_t i = 0; i < coverage_map->size; ++i)
    {
      struct section_coverage *sco = coverage_map->scos + i;
      GElf_Shdr *shdr = &sco->sec->shdr;
      struct coverage *cov = &sco->cov;

      Elf64_Addr s_end = shdr->sh_addr + shdr->sh_size;
      if (end <= shdr->sh_addr || address >= s_end)
	/* no overlap */
	continue;

      if (found && !crosses_boundary)
	{
	  /* While probably not an error, it's very suspicious.  */
	  wr_message (cat | mc_impact_2, where,
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
	  && coverage_is_overlap (cov, cov_begin, cov_end - cov_begin))
	{
	  /* Not a show stopper, this shouldn't derail high-level.  */
	  wr_message (cat | mc_impact_2 | mc_error, where,
		      ": the range %s overlaps with another one.\n",
		      range_fmt (buf, sizeof buf, address, end));
	  overlap = true;
	}

      if (sco->warn)
	wr_message (cat | mc_impact_2, where,
		    ": the range %s covers section %s.\n",
		    range_fmt (buf, sizeof buf, address, end), sco->sec->name);

      /* Section coverage... */
      coverage_add (cov, cov_begin, cov_end - cov_begin);
      sco->hit = true;

      /* And range coverage... */
      coverage_add (&range_cov, r_cov_begin, r_cov_end - r_cov_begin);
    }

  if (!found)
    /* Not a show stopper.  */
    wr_error (where,
	      ": couldn't find a section that the range %s covers.\n",
	      range_fmt (buf, sizeof buf, address, end));
  else if (length > 0)
    {
      bool range_hole (uint64_t h_start, uint64_t h_length,
		       void *user __attribute__ ((unused)))
      {
	char buf2[128];
	assert (h_length != 0);
	wr_error (where,
		  ": portion %s of the range %s "
		  "doesn't fall into any ALLOC section.\n",
		  range_fmt (buf, sizeof buf,
			     h_start + address, h_start + address + h_length),
		  range_fmt (buf2, sizeof buf2, address, end));
	return true;
      }
      coverage_find_holes (&range_cov, 0, length, range_hole, NULL);
    }

  coverage_free (&range_cov);
}

bool
coverage_map_find_holes (struct coverage_map *coverage_map,
			 bool (*cb) (uint64_t begin, uint64_t end,
				     struct section_coverage *, void *),
			 void *user)
{
  for (size_t i = 0; i < coverage_map->size; ++i)
    {
      struct section_coverage *sco = coverage_map->scos + i;

      bool wrap_cb (uint64_t h_start, uint64_t h_length, void *h_user)
      {
	return cb (h_start, h_start + h_length, sco, h_user);
      }

      if (!coverage_find_holes (&sco->cov, 0, sco->sec->shdr.sh_size,
				wrap_cb, user))
	return false;
    }

  return true;
}

void
coverage_map_free (struct coverage_map *coverage_map)
{
  for (size_t i = 0; i < coverage_map->size; ++i)
    coverage_free (&coverage_map->scos[i].cov);
  free (coverage_map->scos);
}


void
cu_free (struct cu *cu_chain)
{
  for (struct cu *it = cu_chain; it != NULL; )
    {
      addr_record_free (&it->die_addrs);

      struct cu *temp = it;
      it = it->next;
      free (temp);
    }
}

static struct cu *
cu_find_cu (struct cu *cu_chain, uint64_t offset)
{
  for (struct cu *it = cu_chain; it != NULL; it = it->next)
    if (it->offset == offset)
      return it;
  return NULL;
}


static bool
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

static bool
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

static bool
read_size_extra (struct read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		 int *offset_sizep, struct where *wh)
{
  if (size32 == DWARF3_LENGTH_64_BIT)
    {
      if (!read_ctx_read_8ubyte (ctx, sizep))
	{
	  wr_error (wh, ": can't read 64bit CU length.\n");
	  return false;
	}

      *offset_sizep = 8;
    }
  else if (size32 >= DWARF3_LENGTH_MIN_ESCAPE_CODE)
    {
      wr_error (wh, ": unrecognized CU length escape value: "
		"%" PRIx32 ".\n", size32);
      return false;
    }
  else
    {
      *sizep = size32;
      *offset_sizep = 4;
    }

  return true;
}

static bool
check_zero_padding (struct read_ctx *ctx,
		    enum message_category category,
		    struct where *wh)
{
  assert (ctx->ptr != ctx->end);
  const unsigned char *save_ptr = ctx->ptr;
  while (!read_ctx_eof (ctx))
    if (*ctx->ptr++ != 0)
      {
	ctx->ptr = save_ptr;
	return false;
      }

  wr_message_padding_0 (category, wh,
			(uint64_t)(save_ptr - ctx->begin),
			(uint64_t)(ctx->end - ctx->begin));
  return true;
}

static struct where
where_from_reloc (struct relocation_data *reloc, struct where *ref)
{
  struct where where
    = WHERE (reloc->type == SHT_REL ? sec_rel : sec_rela, NULL);
  where_reset_1 (&where, reloc->rel[reloc->index].offset);
  where.ref = ref;
  return where;
}

enum skip_type
{
  skip_unref = 0,
  skip_mismatched = 1,
  skip_ok,
};

static struct relocation *
relocation_next (struct relocation_data *reloc, uint64_t offset,
		 struct where *where, enum skip_type st)
{
  if (reloc == NULL || reloc->rel == NULL)
    return NULL;

  while (reloc->index < reloc->size)
    {
      struct relocation *rel = reloc->rel + reloc->index;

      /* This relocation entry is ahead of us.  */
      if (rel->offset > offset)
	return NULL;

      reloc->index++;

      if (rel->invalid)
	continue;

      if (rel->offset < offset)
	{
	  if (st != skip_ok)
	    {
	      struct where reloc_where = where_from_reloc (reloc, where);
	      where_reset_2 (&reloc_where, rel->offset);
	      void (*w) (const struct where *, const char *, ...) = wr_error;
	      (*w) (&reloc_where,
		    ((const char *[])
		    {": relocation targets unreferenced portion of the section.\n",
		     ": relocation is mismatched.\n"})[st]);
	    }
	  continue;
	}

      return rel;
    }

  return NULL;
}

/* Skip all relocation up to offset, and leave cursor pointing at that
   relocation, so that next time relocation_next is called, relocation
   matching that offset is immediately yielded.  */
static void
relocation_skip (struct relocation_data *reloc, uint64_t offset,
		 struct where *where, enum skip_type st)
{
  if (reloc != NULL && reloc->rel != NULL)
    relocation_next (reloc, offset - 1, where, st);
}

/* Skip all the remaining relocations.  */
static void
relocation_skip_rest (struct sec *sec)
{
  if (sec->rel.rel != NULL)
    relocation_next (&sec->rel, (uint64_t)-1, &WHERE (sec->id, NULL),
		     skip_mismatched);
}

/* SYMPTR may be NULL, otherwise (**SYMPTR) has to yield valid memory
   location.  When the function returns, (*SYMPTR) is either NULL, in
   which case we failed or didn't get around to obtain the symbol from
   symbol table, or non-NULL, in which case the symbol was initialized.  */
static void
relocate_one (struct elf_file *file,
	      struct relocation_data *reloc,
	      struct relocation *rel,
	      unsigned width, uint64_t *value, struct where *where,
	      enum section_id offset_into, GElf_Sym **symptr)
{
  if (rel->invalid)
    return;

  struct where reloc_where = where_from_reloc (reloc, where);
  where_reset_2 (&reloc_where, rel->offset);
  struct where reloc_ref_where = reloc_where;
  reloc_ref_where.next = where;

  GElf_Sym symbol_mem, *symbol;
  if (symptr != NULL)
    {
      symbol = *symptr;
      *symptr = NULL;
    }
  else
    symbol = &symbol_mem;

  if (offset_into == sec_invalid)
    {
      wr_message (mc_impact_3 | mc_reloc, &reloc_ref_where,
		  ": relocates a datum that shouldn't be relocated.\n");
      return;
    }

  Elf_Type type = ebl_reloc_simple_type (file->ebl, rel->type);

  unsigned rel_width;
  switch (type)
    {
    case ELF_T_BYTE:
      rel_width = 1;
      break;

    case ELF_T_HALF:
      rel_width = 2;
      break;

    case ELF_T_WORD:
    case ELF_T_SWORD:
      rel_width = 4;
      break;

    case ELF_T_XWORD:
    case ELF_T_SXWORD:
      rel_width = 8;
      break;

    default:
      /* This has already been diagnosed during the isolated
	 validation of relocation section.  */
      return;
    };

  if (rel_width != width)
    wr_error (&reloc_ref_where,
	      ": %d-byte relocation relocates %d-byte datum.\n",
	      rel_width, width);

  /* Tolerate that we might have failed to obtain the symbol table.  */
  if (reloc->symdata != NULL)
    {
      symbol = gelf_getsym (reloc->symdata, rel->symndx, symbol);
      if (symptr != NULL)
	*symptr = symbol;
      if (symbol == NULL)
	{
	  wr_error (&reloc_where,
		    ": couldn't obtain symbol #%d: %s.\n",
		    rel->symndx, elf_errmsg (-1));
	  return;
	}

      uint64_t section_index = symbol->st_shndx;
      /* XXX We should handle SHN_XINDEX here.  Or, instead, maybe it
	 would be possible to use dwfl, which already does XINDEX
	 translation.  */

      /* For ET_REL files, we do section layout manually.  But we
	 don't update symbol table doing that.  So instead of looking
	 at symbol value, look at section address.  */
      uint64_t sym_value = symbol->st_value;
      if (file->ehdr.e_type == ET_REL
	  && ELF64_ST_TYPE (symbol->st_info) == STT_SECTION)
	{
	  assert (sym_value == 0);
	  sym_value = file->sec[section_index].shdr.sh_addr;
	}

      /* It's target value, not section offset.  */
      if (offset_into == rel_value
	  || offset_into == rel_address
	  || offset_into == rel_exec)
	{
	  /* If a target value is what's expected, then complain if
	     it's not either SHN_ABS, an SHF_ALLOC section, or
	     SHN_UNDEF.  For data forms of address_size, an SHN_UNDEF
	     reloc is acceptable, otherwise reject it.  */
	  if (!(section_index == SHN_ABS
		|| (offset_into == rel_address
		    && (section_index == SHN_UNDEF
			|| section_index == SHN_COMMON))))
	    {
	      if (offset_into != rel_address && section_index == SHN_UNDEF)
		wr_error (&reloc_where,
			    ": relocation of an address is formed against SHN_UNDEF symbol"
			    " (symtab index %d).\n", rel->symndx);
	      else
		{
		  GElf_Shdr *shdr = &file->sec[section_index].shdr;
		  if ((shdr->sh_flags & SHF_ALLOC) != SHF_ALLOC)
		    wr_message (mc_reloc | mc_impact_3, &reloc_where,
				": associated section %s isn't SHF_ALLOC.\n",
				file->sec[section_index].name);
		  if (offset_into == rel_exec
		      && (shdr->sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
		    /* This may still be kosher, but it's suspicious.  */
		    wr_message (mc_reloc | mc_impact_2, &reloc_where,
				": relocation against %s is suspicious, expected executable section.\n",
				file->sec[section_index].name);
		}
	    }
	}
      else
	{
	  enum section_id id;
	  /* If symtab[symndx].st_shndx does not match the expected
	     debug section's index, complain.  */
	  if (section_index >= file->size)
	    wr_error (&reloc_where,
		      ": invalid associated section #%" PRId64 ".\n",
		      section_index);
	  else if ((id = file->sec[section_index].id) != offset_into)
	    {
	      char *wh1 = id != sec_invalid
		? strdup (where_fmt (&WHERE (id, NULL), NULL))
		: (char *)file->sec[section_index].name;
	      char *wh2 = strdup (where_fmt (&WHERE (offset_into, NULL), NULL));
	      wr_error (&reloc_where,
			": relocation references section %s, but %s was expected.\n",
			wh1, wh2);
	      free (wh2);
	      if (id != sec_invalid)
		free (wh1);
	    }
	}

      /* Only do the actual relocation if we have ET_REL files.  For
	 non-ET_REL files, only do the above checking.  */
      if (file->ehdr.e_type == ET_REL)
	{
	  *value = rel->addend + sym_value;
	  if (rel_width == 4)
	    *value = *value & (uint64_t)(uint32_t)-1;
	}
    }
}

static enum section_id
reloc_target (uint8_t form, struct abbrev_attrib *at)
{
  switch (form)
    {
    case DW_FORM_strp:
      return sec_str;

    case DW_FORM_addr:

      switch (at->name)
	{
	case DW_AT_low_pc:
	case DW_AT_high_pc:
	case DW_AT_entry_pc:
	  return rel_exec;

	case DW_AT_const_value:
	  /* Appears in some kernel modules.  It's not allowed by the
	     standard, but leave that for high-level checks.  */
	  return rel_address;
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

      switch (at->name)
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

  printf ("XXX don't know how to handle form=%s, at=%s\n",
	  dwarf_form_string (form), dwarf_attr_string (at->name));

  return rel_value;
}

static enum section_id
reloc_target_loc (uint8_t opcode)
{
  switch (opcode)
    {
    case DW_OP_call2:
    case DW_OP_call4:
      return sec_info;

    case DW_OP_addr:
      return rel_address;

    case DW_OP_call_ref:
      assert (!"Can't handle call_ref!");
    };

  printf ("XXX don't know how to handle opcode=%s\n",
	  dwarf_locexpr_opcode_string (opcode));

  return rel_value;
}

static bool
supported_version (unsigned version,
		   size_t num_supported, struct where *where, ...)
{
  bool retval = false;
  va_list ap;
  va_start (ap, where);
  for (size_t i = 0; i < num_supported; ++i)
    {
      unsigned v = va_arg (ap, unsigned);
      if (version == v)
	{
	  retval = true;
	  break;
	}
    }
  va_end (ap);

  if (!retval)
    wr_error (where, ": unsupported version %d.\n", version);

  return retval;
}

static void
check_range_relocations (enum message_category cat,
			 struct where *where,
			 struct elf_file *file,
			 GElf_Sym *begin_symbol,
			 GElf_Sym *end_symbol,
			 const char *description)
{
  if (begin_symbol != NULL
      && end_symbol != NULL
      && begin_symbol->st_shndx != end_symbol->st_shndx)
    wr_message (cat | mc_impact_2 | mc_reloc, where,
		": %s relocated against different sections (%s and %s).\n",
		description,
		file->sec[begin_symbol->st_shndx].name,
		file->sec[end_symbol->st_shndx].name);
}

/*
  Returns:
    -1 in case of error
    +0 in case of no error, but the chain only consisted of a
       terminating zero die.
    +1 in case some dies were actually loaded
 */
static int
read_die_chain (struct elf_file *file,
		struct read_ctx *ctx,
		struct cu *cu,
		struct abbrev_table *abbrevs,
		Elf_Data *strings,
		struct ref_record *local_die_refs,
		struct coverage *strings_coverage,
		struct relocation_data *reloc,
		struct cu_coverage *cu_coverage)
{
  bool got_die = false;
  uint64_t sibling_addr = 0;
  uint64_t die_off, prev_die_off = 0;
  struct abbrev *abbrev = NULL;
  struct abbrev *prev_abbrev = NULL;
  struct where where = WHERE (sec_info, NULL);

  while (!read_ctx_eof (ctx))
    {
      where = cu->where;
      die_off = read_ctx_get_offset (ctx);
      /* Shift reported DIE offset by CU offset, to match the way
	 readelf reports DIEs.  */
      where_reset_2 (&where, die_off + cu->offset);

      uint64_t abbr_code;

      if (!checked_read_uleb128 (ctx, &abbr_code, &where, "abbrev code"))
	return -1;

#define DEF_PREV_WHERE							\
      struct where prev_where = where;					\
      where_reset_2 (&prev_where, prev_die_off + cu->offset)

      /* Check sibling value advertised last time through the loop.  */
      if (sibling_addr != 0)
	{
	  if (abbr_code == 0)
	    wr_error (&where,
		      ": is the last sibling in chain, "
		      "but has a DW_AT_sibling attribute.\n");
	  else if (sibling_addr != die_off)
	    {
	      DEF_PREV_WHERE;
	      wr_error (&prev_where,
			": This DIE claims that its sibling is 0x%"
			PRIx64 ", but it's actually 0x%" PRIx64 ".\n",
			sibling_addr, die_off);
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
	  wr_message (mc_die_rel | mc_acc_suboptimal | mc_impact_4, &prev_where,
		      ": This DIE had children, but no DW_AT_sibling attribute.\n");
	}
#undef DEF_PREV_WHERE

      prev_die_off = die_off;

      /* The section ended.  */
      if (abbr_code == 0)
	break;
      if (read_ctx_eof (ctx))
	{
	  wr_error (&where, ": DIE chain not terminated with DIE with zero abbrev code.\n");
	  break;
	}

      prev_die_off = die_off;
      got_die = true;
      if (dump_die_offsets)
	fprintf (stderr, "%s: abbrev %" PRId64 "\n",
		 where_fmt (&where, NULL), abbr_code);

      /* Find the abbrev matching the code.  */
      prev_abbrev = abbrev;
      abbrev = abbrev_table_find_abbrev (abbrevs, abbr_code);
      if (abbrev == NULL)
	{
	  wr_error (&where,
		    ": abbrev section at 0x%" PRIx64
		    " doesn't contain code %" PRIu64 ".\n",
		    abbrevs->offset, abbr_code);
	  return -1;
	}
      abbrev->used = true;

      addr_record_add (&cu->die_addrs, cu->offset + die_off);

      uint64_t low_pc = (uint64_t)-1, high_pc = (uint64_t)-1;
      bool low_pc_relocated = false, high_pc_relocated = false;
      GElf_Sym low_pc_symbol_mem, *low_pc_symbol = &low_pc_symbol_mem;
      GElf_Sym high_pc_symbol_mem, *high_pc_symbol = &high_pc_symbol_mem;

      /* Attribute values.  */
      for (struct abbrev_attrib *it = abbrev->attribs;
	   it->name != 0; ++it)
	{
	  where.ref = &it->where;

	  uint8_t form = it->form;
	  bool indirect = form == DW_FORM_indirect;
	  if (indirect)
	    {
	      uint64_t value;
	      if (!checked_read_uleb128 (ctx, &value, &where,
					 "indirect attribute form"))
		return -1;

	      if (!attrib_form_valid (value))
		{
		  wr_error (&where,
			    ": invalid indirect form 0x%" PRIx64 ".\n", value);
		  return -1;
		}
	      form = value;

	      if (it->name == DW_AT_sibling)
		switch (check_sibling_form (form))
		  {
		  case -1:
		    wr_message (mc_die_rel | mc_impact_2, &where,
				": DW_AT_sibling attribute with (indirect) form DW_FORM_ref_addr.\n");
		    break;

		  case -2:
		    wr_error (&where,
			      ": DW_AT_sibling attribute with non-reference (indirect) form \"%s\".\n",
			      dwarf_form_string (value));
		  };
	    }

	  void (*value_check_cb) (uint64_t, struct where *) = NULL;

	  /* For checking lineptr, rangeptr, locptr.  */
	  bool check_someptr = false;
	  enum message_category extra_mc = mc_none;

	  /* Callback for local DIE references.  */
	  void check_die_ref_local (uint64_t addr, struct where *who)
	  {
	    assert (ctx->end > ctx->begin);
	    if (addr > (uint64_t)(ctx->end - ctx->begin))
	      {
		wr_error (&where,
			  ": invalid reference outside the CU: 0x%" PRIx64 ".\n",
			  addr);
		return;
	      }

	    if (local_die_refs != NULL)
	      /* Address holds a CU-local reference, so add CU offset
		 to turn it into section offset.  */
	      ref_record_add (local_die_refs, addr += cu->offset, who);
	  }

	  /* Callback for global DIE references.  */
	  void check_die_ref_global (uint64_t addr, struct where *who)
	  {
	    ref_record_add (&cu->die_refs, addr, who);
	  }

	  /* Callback for strp values.  */
	  void check_strp (uint64_t addr, struct where *who)
	  {
	    if (strings == NULL)
	      wr_error (who, ": strp attribute, but no .debug_str data.\n");
	    else if (addr >= strings->d_size)
	      wr_error (who,
			": Invalid offset outside .debug_str: 0x%" PRIx64 ".\n",
			addr);
	    else
	      {
		/* Record used part of .debug_str.  */
		const char *startp = (const char *)strings->d_buf + addr;
		const char *data_end = strings->d_buf + strings->d_size;
		const char *strp = startp;
		while (strp < data_end && *strp != 0)
		  ++strp;
		if (strp == data_end)
		  wr_error (who,
			    ": String at .debug_str: 0x%" PRIx64
			    " is not zero-terminated.\n", addr);

		if (strings_coverage != NULL)
		  coverage_add (strings_coverage, addr, strp - startp + 1);
	      }
	  }

	  /* Callback for rangeptr values.  */
	  void check_rangeptr (uint64_t value, struct where *who)
	  {
	    if ((value % cu->address_size) != 0)
	      wr_message (mc_ranges | mc_impact_2, who,
			  ": rangeptr value %#" PRIx64
			  " not aligned to CU address size.\n", value);
	    cu_coverage->need_ranges = true;
	    ref_record_add (&cu->range_refs, value, who);
	  }

	  /* Callback for lineptr values.  */
	  void check_lineptr (uint64_t value, struct where *who)
	  {
	    ref_record_add (&cu->line_refs, value, who);
	  }

	  /* Callback for locptr values.  */
	  void check_locptr (uint64_t value, struct where *who)
	  {
	    ref_record_add (&cu->loc_refs, value, who);
	  }

	  uint64_t ctx_offset = read_ctx_get_offset (ctx) + cu->offset;
	  bool type_is_rel = file->ehdr.e_type == ET_REL;

	  /* Attribute value.  */
	  uint64_t value = 0;

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
	  size_t width = 0;

	  /* Point to variable that you want to copy relocated value
	     to.  */
	  uint64_t *valuep = NULL;

	  /* Point to variable that you want set to `true' in case the
	     value was relocated.  */
	  bool *relocatedp = NULL;

	  /* Point to variable that you want set to symbol that the
	     relocation was made against.  */
	  GElf_Sym **symbolp = NULL;

	  /* Setup locptr checking.  */
	  if (is_location_attrib (it->name))
	    {
	      switch (form)
		{
		case DW_FORM_data8:
		  if (cu->offset_size == 4)
		    wr_error (&where,
			      ": location attribute with form \"%s\" in 32-bit CU.\n",
			      dwarf_form_string (form));
		  /* fall-through */

		case DW_FORM_data4:
		case DW_FORM_sec_offset:
		  value_check_cb = check_locptr;
		  extra_mc = mc_loc;
		  check_someptr = true;
		  break;

		case DW_FORM_block1:
		case DW_FORM_block2:
		case DW_FORM_block4:
		case DW_FORM_block:
		  break;

		default:
		  /* Only print error if it's indirect.  Otherwise we
		     gave diagnostic during abbrev loading.  */
		  if (indirect)
		    wr_error (&where,
			      ": location attribute with invalid (indirect) form \"%s\".\n",
			      dwarf_form_string (form));
		};
	    }
	  /* Setup rangeptr or lineptr checking.  */
	  else if (it->name == DW_AT_ranges
		   || it->name == DW_AT_stmt_list)
	    switch (form)
	      {
	      case DW_FORM_data8:
		if (cu->offset_size == 4)
		  wr_error (&where,
			    ": %s with form DW_FORM_data8 in 32-bit CU.\n",
			    dwarf_attr_string (it->name));
		/* fall-through */

	      case DW_FORM_data4:
	      case DW_FORM_sec_offset:
		check_someptr = true;
		if (it->name == DW_AT_ranges)
		  {
		    value_check_cb = check_rangeptr;
		    extra_mc = mc_ranges;
		  }
		else
		  {
		    assert (it->name == DW_AT_stmt_list);
		    value_check_cb = check_lineptr;
		    extra_mc = mc_line;
		  }
		break;

	      default:
		/* Only print error if it's indirect.  Otherwise we
		   gave diagnostic during abbrev loading.  */
		if (indirect)
		  wr_error (&where,
			    ": %s with invalid (indirect) form \"%s\".\n",
			    dwarf_attr_string (it->name),
			    dwarf_form_string (form));
	      }
	  /* Setup low_pc and high_pc checking.  */
	  else if (it->name == DW_AT_low_pc)
	    {
	      relocatedp = &low_pc_relocated;
	      symbolp = &low_pc_symbol;
	      valuep = &low_pc;
	    }
	  else if (it->name == DW_AT_high_pc)
	    {
	      relocatedp = &high_pc_relocated;
	      symbolp = &high_pc_symbol;
	      valuep = &high_pc;
	    }

	  /* Load attribute value and setup per-form checking.  */
	  switch (form)
	    {
	    case DW_FORM_strp:
	      value_check_cb = check_strp;
	    case DW_FORM_sec_offset:
	      if (!read_ctx_read_offset (ctx, cu->offset_size == 8, &value))
		{
		cant_read:
		  wr_error (&where, ": can't read value of attribute %s.\n",
			    dwarf_attr_string (it->name));
		  return -1;
		}

	      relocate = rel_require;
	      width = cu->offset_size;
	      break;

	    case DW_FORM_string:
	      if (!read_ctx_read_str (ctx))
		goto cant_read;
	      break;

	    case DW_FORM_ref_addr:
	      value_check_cb = check_die_ref_global;
	      width = cu->offset_size;

	      if (cu->version == 2)
	    case DW_FORM_addr:
		width = cu->address_size;

	      if (!read_ctx_read_offset (ctx, width == 8, &value))
		goto cant_read;

	      /* In non-rel files, neither addr, nor ref_addr /need/
		 a relocation.  */
	      relocate = rel_nonzero;
	      break;

	    case DW_FORM_ref_udata:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_udata:
	      if (!checked_read_uleb128 (ctx, &value, &where,
					 "attribute value"))
		return -1;
	      break;

	    case DW_FORM_flag_present:
	      value = 1;
	      break;

	    case DW_FORM_ref1:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_flag:
	    case DW_FORM_data1:
	      if (!read_ctx_read_var (ctx, 1, &value))
		goto cant_read;
	      break;

	    case DW_FORM_ref2:
	      value_check_cb = check_die_ref_local;
	    case DW_FORM_data2:
	      if (!read_ctx_read_var (ctx, 2, &value))
		goto cant_read;
	      break;

	    case DW_FORM_data4:
	      if (check_someptr)
		{
		  relocate = rel_require;
		  width = 4;
		}
	      if (false)
	    case DW_FORM_ref4:
		value_check_cb = check_die_ref_local;
	      if (!read_ctx_read_var (ctx, 4, &value))
		goto cant_read;
	      break;

	    case DW_FORM_data8:
	      if (check_someptr)
		{
		  relocate = rel_require;
		  width = 8;
		}
	      if (false)
	    case DW_FORM_ref8:
		value_check_cb = check_die_ref_local;
	      if (!read_ctx_read_8ubyte (ctx, &value))
		goto cant_read;
	      break;

	    case DW_FORM_sdata:
	      {
		int64_t value64;
		if (!checked_read_sleb128 (ctx, &value64, &where,
					   "attribute value"))
		  return -1;
		value = (uint64_t) value64;
		break;
	      }

	    case DW_FORM_block:
	      {
		uint64_t length;

		if (false)
	    case DW_FORM_block1:
		  width = 1;

		if (false)
	    case DW_FORM_block2:
		  width = 2;

		if (false)
	    case DW_FORM_block4:
		  width = 4;

		if (width == 0)
		  {
		    if (!checked_read_uleb128 (ctx, &length, &where,
					       "attribute value"))
		      return -1;
		  }
		else if (!read_ctx_read_var (ctx, width, &length))
		  goto cant_read;

		if (is_location_attrib (it->name))
		  {
		    uint64_t expr_start
		      = cu->offset + read_ctx_get_offset (ctx);
		    if (!check_location_expression (file, ctx, cu, expr_start,
						    reloc, length, &where))
		      return -1;
		  }
		else
		  /* xxx really skip_mismatched?  We just don't know
		     how to process these...  */
		  relocation_skip (reloc,
				   read_ctx_get_offset (ctx) + length,
				   &where, skip_mismatched);

		if (!read_ctx_skip (ctx, length))
		  goto cant_read;
		break;
	      }

	    case DW_FORM_indirect:
	      wr_error (&where, ": indirect form is again indirect.\n");
	      return -1;

	    default:
	      wr_error (&where,
			": internal error: unhandled form 0x%x.\n", form);
	    }

	  /* Relocate the value if appropriate.  */
	  struct relocation *rel;
	  if ((rel = relocation_next (reloc, ctx_offset,
				      &where, skip_mismatched)))
	    {
	      if (relocate == rel_no)
		wr_message (mc_impact_4 | mc_die_other | mc_reloc | extra_mc,
			    &where, ": unexpected relocation of %s.\n",
			    dwarf_form_string (form));

	      relocate_one (file, reloc, rel, width, &value, &where,
			    reloc_target (form, it), symbolp);

	      if (relocatedp != NULL)
		*relocatedp = true;
	    }
	  else
	    {
	      if (symbolp != NULL)
		WIPE (*symbolp);
	      if (type_is_rel
		  && (relocate == rel_require
		      || (relocate == rel_nonzero
			  && value != 0)))
		wr_message (mc_impact_2 | mc_die_other | mc_reloc | extra_mc,
			    &where, PRI_LACK_RELOCATION,
			    dwarf_form_string (form));
	    }

	  /* Dispatch value checking.  */
	  if (it->name == DW_AT_sibling)
	    {
	      /* Full-blown DIE reference checking is too heavy-weight
		 and not practical (error messages wise) for checking
		 siblings.  */
	      assert (value_check_cb == check_die_ref_local
		      || value_check_cb == check_die_ref_global);
	      valuep = &sibling_addr;
	    }
	  else if (value_check_cb != NULL)
	    value_check_cb (value, &where);

	  /* Store the relocated value.  Note valuep may point to
	     low_pc or high_pc.  */
	  if (valuep != NULL)
	    *valuep = value;

	  /* Check PC coverage.  */
	  if (abbrev->tag == DW_TAG_compile_unit
	      || abbrev->tag == DW_TAG_partial_unit)
	    {
	      if (it->name == DW_AT_low_pc)
		cu->low_pc = value;

	      if (low_pc != (uint64_t)-1 && high_pc != (uint64_t)-1)
		coverage_add (&cu_coverage->cov, low_pc, high_pc - low_pc);
	    }
	}
      where.ref = NULL;

      if (high_pc != (uint64_t)-1 && low_pc != (uint64_t)-1)
	{
	  if (high_pc_relocated != low_pc_relocated)
	    wr_message (mc_die_other | mc_impact_2 | mc_reloc, &where,
			": only one of DW_AT_low_pc and DW_AT_high_pc is relocated.\n");
	  else
	    check_range_relocations (mc_die_other, &where,
				     file,
				     low_pc_symbol, high_pc_symbol,
				     "DW_AT_low_pc and DW_AT_high_pc");
	}

      where.ref = &abbrev->where;

      if (abbrev->has_children)
	{
	  int st = read_die_chain (file, ctx, cu, abbrevs, strings,
				   local_die_refs,
				   strings_coverage, reloc,
				   cu_coverage);
	  if (st == -1)
	    return -1;
	  else if (st == 0)
	    wr_message (mc_impact_3 | mc_acc_suboptimal | mc_die_rel,
			&where,
			": abbrev has_children, but the chain was empty.\n");
	}
    }

  if (sibling_addr != 0)
    wr_error (&where,
	      ": this DIE should have had its sibling at 0x%"
	      PRIx64 ", but the DIE chain ended.\n", sibling_addr);

  return got_die ? 1 : 0;
}

static bool
read_address_size (struct elf_file *file,
		   struct read_ctx *ctx,
		   uint8_t *address_sizep,
		   struct where *where)
{
  uint8_t address_size;
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      wr_error (where, ": can't read address size.\n");
      return false;
    }

  if (address_size != 4 && address_size != 8)
    {
      /* Keep going.  Deduce the address size from ELF header, and try
	 to parse it anyway.  */
      wr_error (where,
		": invalid address size: %d (only 4 or 8 allowed).\n",
		address_size);
      address_size = file->addr_64 ? 8 : 4;
    }
  else if ((address_size == 8) != file->addr_64)
    /* Keep going, we may still be able to parse it.  */
    wr_error (where,
	      ": CU reports address size of %d in %d-bit ELF.\n",
	      address_size, file->addr_64 ? 64 : 32);

  *address_sizep = address_size;
  return true;
}

static bool
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
    fprintf (stderr, "%s: CU starts\n", where_fmt (&cu->where, NULL));
  bool retval = true;

  /* Version.  */
  uint16_t version;
  if (!read_ctx_read_2ubyte (ctx, &version))
    {
      wr_error (&cu->where, ": can't read version.\n");
      return false;
    }
  if (!supported_version (version, 2, &cu->where, 2, 3))
    return false;
  if (version == 2 && cu->offset_size == 8)
    /* Keep going.  It's a standard violation, but we may still be
       able to read the unit under consideration and do high-level
       checks.  */
    wr_error (&cu->where, ": invalid 64-bit unit in DWARF 2 format.\n");
  cu->version = version;

  /* Abbrev offset.  */
  uint64_t abbrev_offset;
  uint64_t ctx_offset = read_ctx_get_offset (ctx) + cu->offset;
  if (!read_ctx_read_offset (ctx, cu->offset_size == 8, &abbrev_offset))
    {
      wr_error (&cu->where, ": can't read abbrev offset.\n");
      return false;
    }

  struct relocation *rel
    = relocation_next (reloc, ctx_offset, &cu->where, skip_mismatched);
  if (rel != NULL)
    relocate_one (file, reloc, rel, cu->offset_size,
		  &abbrev_offset, &cu->where, sec_abbrev, NULL);
  else if (file->ehdr.e_type == ET_REL)
    wr_message (mc_impact_2 | mc_info | mc_reloc, &cu->where,
		PRI_LACK_RELOCATION, "abbrev offset");

  /* Address size.  */
  {
    uint8_t address_size;
    if (!read_address_size (file, ctx, &address_size, &cu->where))
      return false;
    cu->address_size = address_size;
  }

  /* Look up Abbrev table for this CU.  */
  struct abbrev_table *abbrevs = abbrev_chain;
  for (; abbrevs != NULL; abbrevs = abbrevs->next)
    if (abbrevs->offset == abbrev_offset)
      break;

  if (abbrevs == NULL)
    {
      wr_error (&cu->where,
		": couldn't find abbrev section with offset 0x%" PRIx64 ".\n",
		abbrev_offset);
      return false;
    }

  abbrevs->used = true;

  /* Read DIEs.  */
  struct ref_record local_die_refs;
  WIPE (local_die_refs);

  cu->cudie_offset = read_ctx_get_offset (ctx) + cu->offset;
  if (read_die_chain (file, ctx, cu, abbrevs, strings,
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
		       struct cu_coverage *cu_coverage)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);

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
  while (!read_ctx_eof (&ctx))
    {
      const unsigned char *cu_begin = ctx.ptr;
      struct where where = WHERE (sec_info, NULL);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));

      struct cu *cur = xcalloc (1, sizeof (*cur));
      cur->offset = where.addr1;
      cur->next = cu_chain;
      cur->where = where;
      cur->low_pc = (uint64_t)-1;
      cu_chain = cur;

      uint32_t size32;
      uint64_t size;

      /* Reading CU header is a bit tricky, because we don't know if
	 we have run into (superfluous but allowed) zero padding.  */
      if (!read_ctx_need_data (&ctx, 4)
	  && check_zero_padding (&ctx, mc_info | mc_header, &where))
	break;

      /* CU length.  */
      if (!read_ctx_read_4ubyte (&ctx, &size32))
	{
	  wr_error (&where, ": can't read CU length.\n");
	  success = false;
	  break;
	}
      if (size32 == 0 && check_zero_padding (&ctx, mc_info | mc_header, &where))
	break;

      if (!read_size_extra (&ctx, size32, &size, &cur->offset_size, &where))
	{
	  success = false;
	  break;
	}

      if (!read_ctx_need_data (&ctx, size))
	{
	  wr_error (&where,
		    ": section doesn't have enough data"
		    " to read CU of size %" PRId64 ".\n", size);
	  ctx.ptr = ctx.end;
	  success = false;
	  break;
	}

      const unsigned char *cu_end = ctx.ptr + size;
      cur->length = cu_end - cu_begin; // Length including the length field.

      /* version + debug_abbrev_offset + address_size */
      uint64_t cu_header_size = 2 + cur->offset_size + 1;
      if (size < cu_header_size)
	{
	  wr_error (&where, ": claimed length of %" PRIx64
		    " doesn't even cover CU header.\n", size);
	  success = false;
	  break;
	}
      else
	{
	  /* Make CU context begin just before the CU length, so that DIE
	     offsets are computed correctly.  */
	  struct read_ctx cu_ctx;
	  if (!read_ctx_init_sub (&cu_ctx, &ctx, cu_begin, cu_end))
	    {
	    not_enough:
	      wr_error (&where, PRI_NOT_ENOUGH, "next CU");
	      success = false;
	      break;
	    }
	  cu_ctx.ptr = ctx.ptr;

	  if (!check_cu_structural (file, &cu_ctx, cur, abbrev_chain,
				    strings, strings_coverage, reloc,
				    cu_coverage))
	    {
	      success = false;
	      break;
	    }
	  if (cu_ctx.ptr != cu_ctx.end
	      && !check_zero_padding (&cu_ctx, mc_info, &where))
	    wr_message_padding_n0 (mc_info, &where,
				   read_ctx_get_offset (&ctx),
				   read_ctx_get_offset (&ctx) + size);
	}

      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (success)
    {
      if (ctx.ptr != ctx.end)
	/* Did we read up everything?  */
	wr_message (mc_die_other | mc_impact_4,
		    &WHERE (sec_info, NULL),
		    ": CU lengths don't exactly match Elf_Data contents.");
      else
	/* Did we consume all the relocations?  */
	relocation_skip_rest (sec);

      /* If we managed to read up everything, now do abbrev usage
	 analysis.  */
      for (struct abbrev_table *abbrevs = abbrev_chain;
	   abbrevs != NULL; abbrevs = abbrevs->next)
	{
	  if (!abbrevs->used)
	    {
	      struct where wh = WHERE (sec_abbrev, NULL);
	      where_reset_1 (&wh, abbrevs->offset);
	      wr_message (mc_impact_4 | mc_acc_bloat | mc_abbrevs, &wh,
			  ": abbreviation table is never used.\n");
	    }
	  else if (!abbrevs->skip_check)
	    for (size_t i = 0; i < abbrevs->size; ++i)
	      if (!abbrevs->abbr[i].used)
		wr_message (mc_impact_3 | mc_acc_bloat | mc_abbrevs,
			    &abbrevs->abbr[i].where,
			    ": abbreviation is never used.\n");
	}
    }


  /* We used to check that all CUs have the same address size.  Now
     that we validate address_size of each CU against the ELF header,
     that's not necessary anymore.  */

  bool references_sound = check_global_die_references (cu_chain);
  ref_record_free (&die_refs);

  if (strings_coverage != NULL)
    {
      if (success)
	coverage_find_holes (strings_coverage, 0, strings->d_size, found_hole,
			     &((struct hole_info)
			       {sec_str, mc_strings, strings->d_buf, 0}));
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

static struct coverage_map *
coverage_map_alloc_XA (struct elf_file *elf, bool allow_overlap)
{
  struct coverage_map *ret = xmalloc (sizeof (*ret));
  if (!coverage_map_init (ret, elf,
			  SHF_EXECINSTR | SHF_ALLOC,
			  SHF_ALLOC,
			  allow_overlap))
    {
      free (ret);
      return NULL;
    }
  return ret;
}

static void
coverage_map_free_XA (struct coverage_map *coverage_map)
{
  if (coverage_map != NULL)
    {
      coverage_map_free (coverage_map);
      free (coverage_map);
    }
}

static void
compare_coverage (struct elf_file *file,
		  struct coverage *coverage, struct coverage *other,
		  enum section_id id, char const *what)
{
  struct coverage *cov = coverage_clone (coverage);
  coverage_remove_all (cov, other);

  bool hole (uint64_t start, uint64_t length, void *user)
  {
    /* We need to check alignment vs. the covered section.  Find
       where the hole lies.  */
    struct elf_file *elf = user;
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
	wr_message (mc_aranges | mc_impact_3, &WHERE (id, NULL),
		    ": addresses %s are covered with CUs, but not with %s.\n",
		    range_fmt (buf, sizeof buf, start, start + length), what);
      }

    if (sec == NULL)
      wr_error (NULL, "Couldn't find the section containing the above hole.\n");

    return true;
  }

  coverage_find_ranges (cov, &hole, file);
  coverage_free (cov);
}

/* COVERAGE is portion of address space covered by CUs (either via
   low_pc/high_pc pairs, or via DW_AT_ranges references).  If
   non-NULL, analysis of arange coverage is done against that set. */
bool
check_aranges_structural (struct elf_file *file,
			  struct sec *sec,
			  struct cu *cu_chain,
			  struct coverage *coverage)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);

  bool retval = true;

  struct coverage *aranges_coverage
    = coverage != NULL ? calloc (1, sizeof (struct coverage)) : NULL;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec_aranges, NULL);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));
      const unsigned char *atab_begin = ctx.ptr;

      inline void aranges_coverage_add (uint64_t begin, uint64_t length)
      {
	if (coverage_is_overlap (aranges_coverage, begin, length)
	    && !be_gnu && !be_tolerant)
	  {
	    char buf[128];
	    /* Not a show stopper, this shouldn't derail high-level.  */
	    wr_message (mc_aranges | mc_impact_2 | mc_error, &where,
			": the range %s overlaps with another one.\n",
			range_fmt (buf, sizeof buf, begin, begin + length));
	  }

    	coverage_add (aranges_coverage, begin, length);
      }

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

      struct read_ctx sub_ctx;
      const unsigned char *atab_end = ctx.ptr + size;
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
      if (!supported_version (version, 1, &where, 2))
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
				  &where, skip_mismatched)))
	relocate_one (file, &sec->rel, rel, offset_size,
		      &cu_offset, &where, sec_info, NULL);
      else if (file->ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_aranges | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (cu_chain != NULL && (cu = cu_find_cu (cu_chain, cu_offset)) == NULL)
	wr_error (&where, ": unresolved reference to " PRI_CU ".\n", cu_offset);

      struct where where_cudie;
      if (cu != NULL)
	{
	  where_cudie = WHERE (sec_info, NULL);
	  where_reset_1 (&where_cudie, cu->cudie_offset);
	  where.ref = &where_cudie;
	  where_cudie.formatting = wf_cudie;
	  if (cu->has_arange)
	    wr_message (mc_impact_2 | mc_aranges | mc_header, &where,
			": there has already been arange section for this CU.\n");
	  else
	    cu->has_arange = true;
	}

      /* Address size.  */
      uint8_t address_size;
      if (!read_address_size (file, &sub_ctx, &address_size, &where))
	{
	  retval = false;
	  goto next;
	}

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
	  wr_warning (&where, ": dwarflint can't handle segment_size != 0.\n");
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
	  where_reset_2 (&where, read_ctx_get_offset (&sub_ctx));

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
				      &where, skip_mismatched)))
	    {
	      address_relocated = true;
	      relocate_one (file, &sec->rel, rel, address_size,
			    &address, &where, rel_address, NULL);
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
	  else if (retval && aranges_coverage)
	    aranges_coverage_add (address, length);
	}

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_aranges,
				  &WHERE (where.section, NULL)))
	{
	  wr_message_padding_n0 (mc_aranges | mc_error,
				 &WHERE (where.section, NULL),
				 read_ctx_get_offset (&sub_ctx),
				 read_ctx_get_offset (&sub_ctx) + size);
	  retval = false;
	}

    next:
      if (!read_ctx_skip (&ctx, size))
	/* A "can't happen" error.  */
	goto not_enough;
    }

  if (aranges_coverage != NULL)
    {
      compare_coverage (file, coverage, aranges_coverage,
			sec_aranges, "aranges");
      coverage_free (aranges_coverage);
    }

  return retval;
}

bool
check_pub_structural (struct elf_file *file,
		      struct sec *sec,
		      struct cu *cu_chain)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);
  bool retval = true;

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec->id, NULL);
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

      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, set_begin, set_end))
	{
	not_enough:
	  wr_error (&where, PRI_NOT_ENOUGH, "next set");
	  return false;
	}
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
      if ((rel = relocation_next (&sec->rel, ctx_offset,
				  &where, skip_mismatched)))
	relocate_one (file, &sec->rel, rel, offset_size,
		      &cu_offset, &where, sec_info, NULL);
      else if (file->ehdr.e_type == ET_REL)
	wr_message (mc_impact_2 | mc_pubtables | mc_reloc | mc_header, &where,
		    PRI_LACK_RELOCATION, "debug info offset");

      struct cu *cu = NULL;
      if (cu_chain != NULL && (cu = cu_find_cu (cu_chain, cu_offset)) == NULL)
	wr_error (&where, ": unresolved reference to " PRI_CU ".\n", cu_offset);
      if (cu != NULL)
	{
	  where.ref = &cu->where;
	  bool *has = sec->id == sec_pubnames
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
      if (cu != NULL && cu_len != cu->length)
	{
	  wr_error (&where,
		    ": the table covers length %" PRId64
		    " but CU has length %" PRId64 ".\n", cu_len, cu->length);
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
	      && !addr_record_has_addr (&cu->die_addrs, offset + cu->offset))
	    {
	      wr_error (&where,
			": unresolved reference to " PRI_DIE ".\n", offset);
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

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_pubtables,
				  &WHERE (sec->id, NULL)))
	{
	  wr_message_padding_n0 (mc_pubtables | mc_error,
				 &WHERE (sec->id, NULL),
				 read_ctx_get_offset (&sub_ctx),
				 read_ctx_get_offset (&sub_ctx) + size);
	  retval = false;
	}

    next:
      if (!read_ctx_skip (&ctx, size))
	goto not_enough;
    }

  if (retval)
    relocation_skip_rest (sec);

  return retval;
}


/* Operands are passed back as attribute forms.  In particular,
   DW_FORM_dataX for X-byte operands, DW_FORM_[us]data for
   ULEB128/SLEB128 operands, and DW_FORM_addr for 32b/64b operands.
   If the opcode takes no operands, 0 is passed.

   Return value is false if we couldn't determine (i.e. invalid
   opcode).
 */
static bool
get_location_opcode_operands (uint8_t opcode, uint8_t *op1, uint8_t *op2)
{
  switch (opcode)
    {
#define DW_OP_2(OPCODE, OP1, OP2) \
      case OPCODE: *op1 = OP1; *op2 = OP2; return true;
#define DW_OP_1(OPCODE, OP1) DW_OP_2(OPCODE, OP1, 0)
#define DW_OP_0(OPCODE) DW_OP_2(OPCODE, 0, 0)

      DW_OP_OPERANDS

#undef DEF_DW_OP_2
#undef DEF_DW_OP_1
#undef DEF_DW_OP_0
    default:
      return false;
    };
}

static bool
check_location_expression (struct elf_file *file,
			   struct read_ctx *parent_ctx,
			   struct cu *cu,
			   uint64_t init_off,
			   struct relocation_data *reloc,
			   size_t length,
			   struct where *wh)
{
  struct read_ctx ctx;
  if (!read_ctx_init_sub (&ctx, parent_ctx, parent_ctx->ptr,
			  parent_ctx->ptr + length))
    {
      wr_error (wh, PRI_NOT_ENOUGH, "location expression");
      return false;
    }

  struct ref_record oprefs;
  WIPE (oprefs);

  struct addr_record opaddrs;
  WIPE (opaddrs);

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec_locexpr, wh);
      uint64_t opcode_off = read_ctx_get_offset (&ctx) + init_off;
      where_reset_1 (&where, opcode_off);
      addr_record_add (&opaddrs, opcode_off);

      uint8_t opcode;
      if (!read_ctx_read_ubyte (&ctx, &opcode))
	{
	  wr_error (&where, ": can't read opcode.\n");
	  break;
	}

      uint8_t op1, op2;
      if (!get_location_opcode_operands (opcode, &op1, &op2))
	{
	  wr_error (&where, ": can't decode opcode \"%s\".\n",
		    dwarf_locexpr_opcode_string (opcode));
	  break;
	}

#define READ_FORM(OP, STR, PTR)						\
      do {								\
	if (OP != 0)							\
	  {								\
	    uint64_t _off = read_ctx_get_offset (&ctx) + init_off;	\
	    uint64_t *_ptr = (PTR);					\
	    if (!read_ctx_read_form (&ctx, cu, (OP),			\
				     _ptr, &where, STR " operand"))	\
	      {								\
		wr_error (&where, ": opcode \"%s\""			\
			  ": can't read " STR " operand (form \"%s\").\n", \
			  dwarf_locexpr_opcode_string (opcode),		\
			  dwarf_form_string ((OP)));			\
		goto out;						\
	      }								\
	    struct relocation *_rel;					\
	    if ((_rel = relocation_next (reloc, _off,			\
					 &where, skip_mismatched)))	\
	      relocate_one (file, reloc, _rel,				\
			    cu->address_size, _ptr, &where,		\
			    reloc_target_loc (opcode), NULL);		\
	  }								\
      } while (0)

      uint64_t value1, value2;
      READ_FORM (op1, "1st", &value1);
      READ_FORM (op2, "2st", &value2);
#undef READ_FORM

      switch (opcode)
	{
	case DW_OP_bra:
	case DW_OP_skip:
	  {
	    int16_t skip = (uint16_t)value1;

	    if (skip == 0)
	      wr_message (mc_loc | mc_acc_bloat | mc_impact_3, &where,
			  ": %s with skip 0.\n",
			  dwarf_locexpr_opcode_string (opcode));
	    else if (skip > 0 && !read_ctx_need_data (&ctx, (size_t)skip))
	      wr_error (&where, ": %s branches out of location expression.\n",
			dwarf_locexpr_opcode_string (opcode));
	    /* Compare with the offset after the two-byte skip value.  */
	    else if (skip < 0 && ((uint64_t)-skip) > read_ctx_get_offset (&ctx))
	      wr_error (&where,
			": %s branches before the beginning of location expression.\n",
			dwarf_locexpr_opcode_string (opcode));
	    else
	      ref_record_add (&oprefs, opcode_off + skip, &where);

	    break;
	  }

	case DW_OP_const8u:
	case DW_OP_const8s:
	  if (cu->address_size == 4)
	    wr_error (&where, ": %s on 32-bit machine.\n",
		      dwarf_locexpr_opcode_string (opcode));
	  break;

	default:
	  if (cu->address_size == 4
	      && (opcode == DW_OP_constu
		  || opcode == DW_OP_consts
		  || opcode == DW_OP_deref_size
		  || opcode == DW_OP_plus_uconst)
	      && (value1 > (uint64_t)(uint32_t)-1))
	    wr_message (mc_loc | mc_acc_bloat | mc_impact_3, &where,
			": %s with operand %#" PRIx64 " on 32-bit machine.\n",
			dwarf_locexpr_opcode_string (opcode), value1);
	};
    }

 out:
  for (size_t i = 0; i < oprefs.size; ++i)
    {
      struct ref *ref = oprefs.refs + i;
      if (!addr_record_has_addr (&opaddrs, ref->addr))
	wr_error (&ref->who,
		  ": unresolved reference to opcode at %#" PRIx64 ".\n",
		  ref->addr);
    }

  addr_record_free (&opaddrs);
  ref_record_free (&oprefs);

  return true;
}

static bool
check_loc_or_range_ref (struct elf_file *file,
			const struct read_ctx *parent_ctx,
			struct cu *cu,
			struct sec *sec,
			struct coverage *coverage,
			struct coverage_map *coverage_map,
			struct cu_coverage *cu_coverage,
			uint64_t addr,
			struct where *wh,
			enum message_category cat)
{
  char buf[128]; // messages

  assert (sec->id == sec_loc || sec->id == sec_ranges);
  assert (cat == mc_loc || cat == mc_ranges);
  assert ((sec->id == sec_loc) == (cat == mc_loc));
  assert (coverage != NULL);

  struct read_ctx ctx;
  read_ctx_init (&ctx, parent_ctx->file, parent_ctx->data);
  if (!read_ctx_skip (&ctx, addr))
    {
      wr_error (wh, ": invalid reference outside the section "
		"%#" PRIx64 ", size only %#tx.\n",
		addr, ctx.end - ctx.begin);
      return false;
    }

  bool retval = true;
  bool contains_locations = sec->id == sec_loc;

  if (coverage_is_covered (coverage, addr, 1))
    {
      wr_error (wh, ": reference to %#" PRIx64
		" points into another location or range list.\n", addr);
      retval = false;
    }

  uint64_t escape = cu->address_size == 8
    ? (uint64_t)-1 : (uint64_t)(uint32_t)-1;

  bool overlap = false;
  uint64_t base = cu->low_pc;
  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec->id, wh);
      where_reset_1 (&where, read_ctx_get_offset (&ctx));

#define HAVE_OVERLAP						\
      do {							\
	wr_error (&where, ": range definitions overlap.\n");	\
	retval = false;						\
	overlap = true;						\
      } while (0)

      /* begin address */
      uint64_t begin_addr;
      uint64_t begin_off = read_ctx_get_offset (&ctx);
      GElf_Sym begin_symbol_mem, *begin_symbol = &begin_symbol_mem;
      bool begin_relocated = false;
      if (!overlap
	  && coverage_is_overlap (coverage, begin_off, cu->address_size))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, cu->address_size == 8, &begin_addr))
	{
	  wr_error (&where, ": can't read address range beginning.\n");
	  return false;
	}

      struct relocation *rel;
      if ((rel = relocation_next (&sec->rel, begin_off,
				  &where, skip_mismatched)))
	{
	  begin_relocated = true;
	  relocate_one (file, &sec->rel, rel, cu->address_size,
			&begin_addr, &where, rel_value,	&begin_symbol);
	}

      /* end address */
      uint64_t end_addr;
      uint64_t end_off = read_ctx_get_offset (&ctx);
      GElf_Sym end_symbol_mem, *end_symbol = &end_symbol_mem;
      bool end_relocated = false;
      if (!overlap
	  && coverage_is_overlap (coverage, end_off, cu->address_size))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, cu->address_size == 8, &end_addr))
	{
	  wr_error (&where, ": can't read address range ending.\n");
	  return false;
	}

      if ((rel = relocation_next (&sec->rel, end_off,
				  &where, skip_mismatched)))
	{
	  end_relocated = true;
	  relocate_one (file, &sec->rel, rel, cu->address_size,
			&end_addr, &where, rel_value, &end_symbol);
	  if (begin_addr != escape)
	    {
	      if (!begin_relocated)
		wr_message (cat | mc_impact_2 | mc_reloc, &where,
			    ": end of address range is relocated, but the beginning wasn't.\n");
	      else
		check_range_relocations (cat, &where, file,
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
	    wr_message (cat | mc_error, &where,	": has negative range %s.\n",
			range_fmt (buf, sizeof buf, begin_addr, end_addr));
	  else if (begin_addr == end_addr)
	    /* 2.6.6: A location list entry [...] whose beginning
	       and ending addresses are equal has no effect.  */
	    wr_message (cat | mc_acc_bloat | mc_impact_3, &where,
			": entry covers no range.\n");
	  /* Skip coverage analysis if we have errors or have no base
	     (or just don't do coverage analysis at all).  */
	  else if (base < (uint64_t)-2 && retval
		   && (coverage_map != NULL || cu_coverage != NULL))
	    {
	      uint64_t address = begin_addr + base;
	      uint64_t length = end_addr - begin_addr;
	      if (coverage_map != NULL)
		coverage_map_add (coverage_map, address, length, &where, cat);
	      if (cu_coverage != NULL)
		coverage_add (&cu_coverage->cov, address, length);
	    }

	  if (contains_locations)
	    {
	      /* location expression length */
	      uint16_t len;
	      if (!overlap
		  && coverage_is_overlap (coverage,
					  read_ctx_get_offset (&ctx), 2))
		HAVE_OVERLAP;

	      if (!read_ctx_read_2ubyte (&ctx, &len))
		{
		  wr_error (&where, ": can't read length of location expression.\n");
		  return false;
		}

	      /* location expression itself */
	      uint64_t expr_start = read_ctx_get_offset (&ctx);
	      if (!check_location_expression (file, &ctx, cu, expr_start,
					      &sec->rel, len, &where))
		return false;
	      uint64_t expr_end = read_ctx_get_offset (&ctx);
	      if (!overlap
		  && coverage_is_overlap (coverage,
					  expr_start, expr_end - expr_start))
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

      coverage_add (coverage, where.addr1, read_ctx_get_offset (&ctx) - where.addr1);
      if (done)
	break;
    }

  return retval;
}

bool
check_loc_or_range_structural (struct elf_file *file,
			       struct sec *sec,
			       struct cu *cu_chain,
			       struct cu_coverage *cu_coverage)
{
  assert (sec->id == sec_loc || sec->id == sec_ranges);
  assert (cu_chain != NULL);

  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);

  bool retval = true;

  /* For .debug_ranges, we optionally do ranges vs. ELF sections
     coverage analysis.  */
  struct coverage_map *coverage_map = NULL;
  if (do_range_coverage && sec->id == sec_ranges
      && (coverage_map
	    = coverage_map_alloc_XA (file, sec->id == sec_loc)) == NULL)
    {
      wr_error (&WHERE (sec->id, NULL),
		": couldn't read ELF, skipping coverage analysis.\n");
      retval = false;
    }

  /* Overlap discovery.  */
  struct coverage coverage;
  WIPE (coverage);

  enum message_category cat = sec->id == sec_loc ? mc_loc : mc_ranges;

  /* Relocation checking in the followings assumes that all the
     references are organized in monotonously increasing order.  That
     doesn't have to be the case.  So merge all the references into
     one sorted array.  */
  size_t size = 0;
  for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
    {
      struct ref_record *rec
	= sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
      size += rec->size;
    }
  struct ref_cu
  {
    struct ref ref;
    struct cu *cu;
  };
  struct ref_cu *refs = xmalloc (sizeof (*refs) * size);
  struct ref_cu *refptr = refs;
  for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
    {
      struct ref_record *rec
	= sec->id == sec_loc ? &cu->loc_refs : &cu->range_refs;
      for (size_t i = 0; i < rec->size; ++i)
	*refptr++ = ((struct ref_cu){.ref = rec->refs[i], .cu = cu});
    }
  int compare_refs (const void *a, const void *b)
  {
    const struct ref_cu *ref_a = (const struct ref_cu *)a;
    const struct ref_cu *ref_b = (const struct ref_cu *)b;

    if (ref_a->ref.addr > ref_b->ref.addr)
      return 1;
    else if (ref_a->ref.addr < ref_b->ref.addr)
      return -1;
    else
      return 0;
  }
  qsort (refs, size, sizeof (*refs), compare_refs);

  uint64_t last_off = 0;
  for (size_t i = 0; i < size; ++i)
    {
      uint64_t off = refs[i].ref.addr;
      if (i > 0)
	{
	  if (off == last_off)
	    continue;
	  relocation_skip (&sec->rel, off,
			   &WHERE (sec->id, NULL), skip_unref);
	}

      /* XXX We pass cu_coverage down for all ranges.  That means all
	 ranges get recorded, not only those belonging to CUs.
	 Perhaps that's undesirable.  */
      if (!check_loc_or_range_ref (file, &ctx, refs[i].cu, sec,
				   &coverage, coverage_map,
				   sec->id == sec_ranges ? cu_coverage : NULL,
				   off, &refs[i].ref.who, cat))
	retval = false;
      last_off = off;
    }

  if (retval)
    {
      relocation_skip_rest (sec);

      /* We check that all CUs have the same address size when building
	 the CU chain.  So just take the address size of the first CU in
	 chain.  */
      coverage_find_holes (&coverage, 0, ctx.data->d_size, found_hole,
			   &((struct hole_info)
			     {sec->id, cat, ctx.data->d_buf,
			      cu_chain->address_size}));

      if (coverage_map)
	coverage_map_find_holes (coverage_map, &coverage_map_found_hole,
				 &(struct coverage_map_hole_info)
				 {coverage_map->elf, {sec->id, cat, NULL, 0}});
    }

  coverage_free (&coverage);
  coverage_map_free_XA (coverage_map);

  if (retval && cu_coverage != NULL)
    /* Only drop the flag if we were successful, so that the coverage
       analysis isn't later done against incomplete data.  */
    cu_coverage->need_ranges = false;

  return retval;
}

static GElf_Rela *
get_rel_or_rela (Elf_Data *data, int ndx,
		 GElf_Rela *dst, size_t type)
{
  if (type == SHT_RELA)
    return gelf_getrela (data, ndx, dst);
  else
    {
      assert (type == SHT_REL);
      GElf_Rel rel_mem;
      if (gelf_getrel (data, ndx, &rel_mem) == NULL)
	return NULL;
      dst->r_offset = rel_mem.r_offset;
      dst->r_info = rel_mem.r_info;
      dst->r_addend = 0;
      return dst;
    }
}

bool
read_rel (struct elf_file *file,
	  struct sec *sec,
	  Elf_Data *reldata,
	  bool elf_64)
{
  assert (sec->rel.type == SHT_REL
	  || sec->rel.type == SHT_RELA);
  bool is_rela = sec->rel.type == SHT_RELA;

  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);

  size_t entrysize
    = elf_64
    ? (is_rela ? sizeof (Elf64_Rela) : sizeof (Elf64_Rel))
    : (is_rela ? sizeof (Elf32_Rela) : sizeof (Elf32_Rel));
  size_t count = reldata->d_size / entrysize;

  struct where parent = WHERE (sec->id, NULL);
  struct where where = WHERE (is_rela ? sec_rela : sec_rel, NULL);
  where.ref = &parent;

  for (unsigned i = 0; i < count; ++i)
    {
      where_reset_1 (&where, i);

      REALLOC (&sec->rel, rel);
      struct relocation *cur = sec->rel.rel + sec->rel.size++;
      WIPE (*cur);

      GElf_Rela rela_mem, *rela
	= get_rel_or_rela (reldata, i, &rela_mem, sec->rel.type);
      if (rela == NULL)
	{
	  wr_error (&where, ": couldn't read relocation.\n");
	skip:
	  cur->invalid = true;
	  continue;
	}

      int cur_type = GELF_R_TYPE (rela->r_info);
      if (cur_type == 0) /* No relocation.  */
	{
	  wr_message (mc_impact_3 | mc_reloc | mc_acc_bloat, &where,
		      ": NONE relocation is superfluous.\n");
	  goto skip;
	}

      cur->offset = rela->r_offset;
      cur->symndx = GELF_R_SYM (rela->r_info);
      cur->type = cur_type;

      where_reset_2 (&where, cur->offset);

      Elf_Type type = ebl_reloc_simple_type (file->ebl, cur->type);
      int width;

      switch (type)
	{
	case ELF_T_WORD:
	case ELF_T_SWORD:
	  width = 4;
	  break;

	case ELF_T_XWORD:
	case ELF_T_SXWORD:
	  width = 8;
	  break;

	case ELF_T_BYTE:
	case ELF_T_HALF:
	  /* Technically legal, but never used.  Better have dwarflint
	     flag them as erroneous, because it's more likely these
	     are a result of a bug than actually being used.  */
	  {
	    char buf[64];
	    wr_error (&where, ": 8 or 16-bit relocation type %s.\n",
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }

	default:
	  {
	    char buf[64];
	    wr_error (&where, ": invalid relocation %d (%s).\n",
		      cur->type,
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }
	};

      if (cur->offset + width >= sec->data->d_size)
	{
	  wr_error (&where,
		    ": relocation doesn't fall into relocated section.\n");
	  goto skip;
	}

      uint64_t value;
      if (width == 4)
	value = dwarflint_read_4ubyte_unaligned
	  (file, sec->data->d_buf + cur->offset);
      else
	{
	  assert (width == 8);
	  value = dwarflint_read_8ubyte_unaligned
	    (file, sec->data->d_buf + cur->offset);
	}

      if (is_rela)
	{
	  if (value != 0)
	    wr_message (mc_impact_2 | mc_reloc, &where,
			": SHR_RELA relocates a place with non-zero value (addend=%#"
			PRIx64", value=%#"PRIx64").\n", rela->r_addend, value);
	  cur->addend = rela->r_addend;
	}
      else
	cur->addend = value;
    }

  /* Sort the reloc section so that the applicable addresses of
     relocation entries are monotonously increasing.  */
  int compare (const void *a, const void *b)
  {
    return ((struct relocation *)a)->offset
      - ((struct relocation *)b)->offset;
  }

  qsort (sec->rel.rel, sec->rel.size,
	 sizeof (*sec->rel.rel), &compare);
  return true;
}

bool
check_line_structural (struct elf_file *file,
		       struct sec *sec,
		       struct cu *cu_chain)
{
  struct read_ctx ctx;
  read_ctx_init (&ctx, file, sec->data);
  bool retval = true;

  struct addr_record line_tables;
  WIPE (line_tables);

  while (!read_ctx_eof (&ctx))
    {
      struct where where = WHERE (sec->id, NULL);
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
	  wr_error (&where, ": can't read table length.\n");
	  return false;
	}
      if (!read_size_extra (&ctx, size32, &size, &offset_size, &where))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx.ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, &ctx, set_begin, set_end))
	{
	not_enough:
	  wr_error (&where, PRI_NOT_ENOUGH, "next unit");
	  return false;
	}
      sub_ctx.ptr = ctx.ptr;
      sub_ctx.begin = ctx.begin;

      {
      /* Version.  */
      uint16_t version;
      if (!read_ctx_read_2ubyte (&sub_ctx, &version))
	{
	  wr_error (&where, ": can't read set version.\n");
	skip:
	  retval = false;
	  goto next;
	}
      if (!supported_version (version, 2, &where, 2, 3))
	goto skip;

      /* Header length.  */
      uint64_t header_length;
      if (!read_ctx_read_offset (&sub_ctx, offset_size == 8, &header_length))
	{
	  wr_error (&where, ": can't read attribute value.\n");
	  goto skip;
	}
      const unsigned char *program_start = sub_ctx.ptr + header_length;

      /* Minimum instruction length.  */
      uint8_t minimum_i_length;
      if (!read_ctx_read_ubyte (&sub_ctx, &minimum_i_length))
	{
	  wr_error (&where, ": can't read minimum instruction length.\n");
	  goto skip;
	}

      /* Default value of is_stmt.  */
      uint8_t default_is_stmt;
      if (!read_ctx_read_ubyte (&sub_ctx, &default_is_stmt))
	{
	  wr_error (&where, ": can't read default_is_stmt.\n");
	  goto skip;
	}
      /* 7.21: The boolean values "true" and "false" used by the line
	 number information program are encoded as a single byte
	 containing the value 0 for "false," and a non-zero value for
	 "true."  [But give a notice if it's not 0 or 1.]  */
      if (default_is_stmt != 0
	  && default_is_stmt != 1)
	wr_message (mc_line | mc_impact_2 | mc_header, &where,
		    ": default_is_stmt should be 0 or 1, not %ud\n",
		    default_is_stmt);

      /* Line base.  */
      int8_t line_base;
      if (!read_ctx_read_ubyte (&sub_ctx, (uint8_t *)&line_base))
	{
	  wr_error (&where, ": can't read line_base.\n");
	  goto skip;
	}

      /* Line range.  */
      uint8_t line_range;
      if (!read_ctx_read_ubyte (&sub_ctx, &line_range))
	{
	  wr_error (&where, ": can't read line_range.\n");
	  goto skip;
	}

      /* Opcode base.  */
      uint8_t opcode_base;
      if (!read_ctx_read_ubyte (&sub_ctx, &opcode_base))
	{
	  wr_error (&where, ": can't read opcode_base.\n");
	  goto skip;
	}

      /* Standard opcode lengths.  */
      if (opcode_base == 0)
	{
	  wr_error (&where, ": opcode base set to 0.\n");
	  opcode_base = 1; // so that in following, our -1s don't underrun
	}
      uint8_t std_opc_lengths[opcode_base - 1]; /* -1, opcodes go from 1.  */
      for (unsigned i = 0; i < (unsigned)(opcode_base - 1); ++i)
	if (!read_ctx_read_ubyte (&sub_ctx, std_opc_lengths + i))
	  {
	    wr_error (&where,
		      ": can't read length of standard opcode #%d.\n", i);
	    goto skip;
	  }

      /* Include directories.  */
      struct include_directory_t
      {
	const char *name;
	bool used;
      };
      struct include_directories_t
      {
	size_t size;
	size_t alloc;
	struct include_directory_t *dirs;
      } include_directories;
      WIPE (include_directories);

      while (!read_ctx_eof (&sub_ctx))
	{
	  const char *name = read_ctx_read_str (&sub_ctx);
	  if (name == NULL)
	    {
	      wr_error (&where,
			": can't read name of include directory #%zd.\n",
			include_directories.size + 1); /* Numbered from 1.  */
	      goto skip;
	    }
	  if (*name == 0)
	    break;

	  REALLOC (&include_directories, dirs);
	  include_directories.dirs[include_directories.size++] =
	    (struct include_directory_t){name, false};
	}

      /* File names.  */
      struct file_t
      {
	const char *name;
	uint64_t dir_idx;
	bool used;
      };
      struct files_t
      {
	size_t size;
	size_t alloc;
	struct file_t *files;
      } files;
      WIPE (files);

      /* Directory index.  */
      bool read_directory_index (const char *name, uint64_t *ptr)
      {
	if (!checked_read_uleb128 (&sub_ctx, ptr,
				   &where, "directory index"))
	  return false;
	if (*name == '/' && *ptr != 0)
	  wr_message (mc_impact_2 | mc_line | mc_header, &where,
		      ": file #%zd has absolute pathname, but refers to directory != 0.\n",
		      files.size + 1);
	if (*ptr > include_directories.size) /* Not >=, dirs indexed from 1.  */
	  {
	    wr_message (mc_impact_4 | mc_line | mc_header, &where,
			": file #%zd refers to directory #%" PRId64 ", which wasn't defined.\n",
			files.size + 1, *ptr);
	    /* Consumer might choke on that.  */
	    retval = false;
	  }
	else if (*ptr != 0)
	  include_directories.dirs[*ptr - 1].used = true;
	return true;
      }

      while (1)
	{
	  const char *name = read_ctx_read_str (&sub_ctx);
	  if (name == NULL)
	    {
	      wr_error (&where,
			": can't read name of file #%zd.\n",
			files.size + 1); /* Numbered from 1.  */
	      goto skip;
	    }
	  if (*name == 0)
	    break;

	  uint64_t dir_idx;
	  if (!read_directory_index (name, &dir_idx))
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

	  REALLOC (&files, files);
	  files.files[files.size++]
	    = (struct file_t){name, dir_idx, false};
	}

      /* Skip the rest of the header.  */
      if (sub_ctx.ptr > program_start)
	{
	  wr_error (&where,
		    ": header claims that it has a size of %#" PRIx64
		    ", but in fact it has a size of %#" PRIx64 ".\n",
		    header_length, sub_ctx.ptr - program_start + header_length);
	  /* Assume that the header lies, and what follows is in
	     fact line number program.  */
	  retval = false;
	}
      else if (sub_ctx.ptr < program_start)
	{
	  if (!check_zero_padding (&sub_ctx, mc_line | mc_header, &where))
	    wr_message_padding_n0 (mc_line | mc_header, &WHERE (sec_line, NULL),
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
	      wr_error (&where, ": can't read opcode.\n");
	      goto skip;
	    }

	  void use_file (uint64_t file_idx)
	  {
	    if (file_idx == 0 || file_idx > files.size)
	      {
		wr_error (&where,
			  ": DW_LNS_set_file: invalid file index %" PRId64 ".\n",
			  file_idx);
		retval = false;
	      }
	    else
	      files.files[file_idx - 1].used = true;
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
		    wr_error (&where, ": can't read extended opcode.\n");
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
 		      if (!read_ctx_read_offset (&sub_ctx,
						 file->addr_64, &addr))
			{
			  wr_error (&where, ": can't read operand of DW_LNE_set_address.\n");
			  goto skip;
			}

		      struct relocation *rel;
		      if ((rel = relocation_next (&sec->rel, ctx_offset,
						  &where, skip_mismatched)))
			relocate_one (file, &sec->rel, rel,
				      file->addr_64 ? 8 : 4,
				      &addr, &where, rel_address, NULL);
		      else if (file->ehdr.e_type == ET_REL)
			wr_message (mc_impact_2 | mc_line | mc_reloc, &where,
				    PRI_LACK_RELOCATION, "DW_LNE_set_address");
		      break;
		    }

		  case DW_LNE_define_file:
		    {
		      const char *name;
		      if ((name = read_ctx_read_str (&sub_ctx)) == NULL)
			{
			  wr_error (&where,
				    ": can't read filename operand of DW_LNE_define_file.\n");
			  goto skip;
			}
		      uint64_t dir_idx;
		      if (!read_directory_index (name, &dir_idx))
			goto skip;
		      REALLOC (&files, files);
		      files.files[files.size++] =
			(struct file_t){name, dir_idx, false};
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
			wr_message (mc_impact_2 | mc_line, &where,
				    ": unknown extended opcode #%d.\n", extended);
		      };
		  };

		if (sub_ctx.ptr > next)
		  {
		    wr_error (&where,
			      ": opcode claims that it has a size of %#" PRIx64
			      ", but in fact it has a size of %#" PRIx64 ".\n",
			      skip_len, skip_len + (next - sub_ctx.ptr));
		    retval = false;
		  }
		else if (sub_ctx.ptr < next)
		  {
		    if (handled
			&& !check_zero_padding (&sub_ctx, mc_line, &where))
		      wr_message_padding_n0 (mc_line, &WHERE (sec_line, NULL),
					     read_ctx_get_offset (&sub_ctx),
					     next - sub_ctx.begin);
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
		    wr_error (&where, ": can't read operand of DW_LNS_fixed_advance_pc.\n");
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
		use_file (file_idx);
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
		    wr_message (mc_impact_2 | mc_line, &where,
				": unknown standard opcode #%d.\n", opcode);
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
	      use_file (1);
	      first_file = false;
	    }

	  if (opcode != 0 || extended != DW_LNE_end_sequence)
	    seen_opcode = true;
	}

      for (size_t i = 0; i < include_directories.size; ++i)
	if (!include_directories.dirs[i].used)
	  wr_message (mc_impact_3 | mc_acc_bloat | mc_line | mc_header,
		      &where, ": the include #%zd `%s' is not used.\n",
		      i + 1, include_directories.dirs[i].name);

      for (size_t i = 0; i < files.size; ++i)
	if (!files.files[i].used)
	  wr_message (mc_impact_3 | mc_acc_bloat | mc_line | mc_header,
		      &where, ": the file #%zd `%s' is not used.\n",
		      i + 1, files.files[i].name);

      if (!seen_opcode)
	wr_message (mc_line | mc_acc_bloat | mc_impact_3, &where,
		    ": empty line number program.\n");
      if (!terminated)
	wr_error (&where,
		  ": sequence of opcodes not terminated with DW_LNE_end_sequence.\n");
      else if (sub_ctx.ptr != sub_ctx.end
	       && !check_zero_padding (&sub_ctx, mc_line,
				       &WHERE (sec_line, NULL)))
	wr_message_padding_n0 (mc_line, &WHERE (sec_line, NULL),
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

  if (retval)
    {
      relocation_skip_rest (sec);

      for (struct cu *cu = cu_chain; cu != NULL; cu = cu->next)
	for (size_t i = 0; i < cu->line_refs.size; ++i)
	  {
	    struct ref *ref = cu->line_refs.refs + i;
	    if (!addr_record_has_addr (&line_tables, ref->addr))
	      wr_error (&ref->who,
			": unresolved reference to .debug_line table %#" PRIx64 ".\n",
			ref->addr);
	  }
    }

  return retval;
}
