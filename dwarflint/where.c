/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009,2010 Red Hat, Inc.
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

#include "where.h"

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern bool show_refs (void);

const char *
where_fmt (const struct where *wh, char *ptr)
{
  if (wh == NULL)
    return "";

  static char buf[256];

  struct section_info
  {
    const char *name;
    const char *addr1n;
    const char *addr1f;
    const char *addr2n;
    const char *addr2f;
    const char *addr3n;
    const char *addr3f;
  };

  static struct section_info section_names[] =
    {
      [sec_info] = {".debug_info", "CU", "%"PRId64,
		    "DIE", "%#"PRIx64, NULL, NULL},

      [sec_abbrev] = {".debug_abbrev", "section", "%"PRId64,
		      "abbreviation", "%"PRId64, "abbr. attribute", "%#"PRIx64},

      [sec_aranges] = {".debug_aranges", "table", "%"PRId64,
		       "arange", "%#"PRIx64, NULL, NULL},

      [sec_pubnames] = {".debug_pubnames", "pubname table", "%"PRId64,
			"pubname", "%#"PRIx64, NULL, NULL},

      [sec_pubtypes] = {".debug_pubtypes", "pubtype table", "%"PRId64,
			"pubtype", "%#"PRIx64, NULL, NULL},

      [sec_str] = {".debug_str", "offset", "%#"PRIx64,
		   NULL, NULL, NULL, NULL},

      [sec_line] = {".debug_line", "table", "%"PRId64,
		    "offset", "%#"PRIx64, NULL, NULL},

      [sec_loc] = {".debug_loc", "loclist", "%#"PRIx64,
		   "offset", "%#"PRIx64, NULL, NULL},

      [sec_mac] = {".debug_mac", NULL, NULL, NULL, NULL, NULL, NULL},

      [sec_ranges] = {".debug_ranges", "rangelist", "%#"PRIx64,
		      "offset", "%#"PRIx64, NULL, NULL},

      [sec_locexpr] = {"location expression", "offset", "%#"PRIx64,
		       NULL, NULL, NULL, NULL},

      [sec_rel] = {".rel", "relocation", "%"PRId64,
		   "offset", "%#"PRIx64, NULL, NULL},
      [sec_rela] = {".rela", "relocation", "%"PRId64,
		    "offset", "%#"PRIx64, NULL, NULL},
    };

  static struct section_info special_formats[] =
    {
      [wf_cudie] = {".debug_info", "CU DIE", "%"PRId64, NULL, NULL, NULL, NULL}
    };

  assert (wh->section < sizeof (section_names) / sizeof (*section_names));
  struct section_info *inf
    = (wh->formatting == wf_plain)
    ? section_names + wh->section
    : special_formats + wh->formatting;

  assert (inf->name);

  assert ((inf->addr1n == NULL) == (inf->addr1f == NULL));
  assert ((inf->addr2n == NULL) == (inf->addr2f == NULL));
  assert ((inf->addr3n == NULL) == (inf->addr3f == NULL));

  assert ((wh->addr1 != (uint64_t)-1) ? inf->addr1n != NULL : true);
  assert ((wh->addr2 != (uint64_t)-1) ? inf->addr2n != NULL : true);
  assert ((wh->addr3 != (uint64_t)-1) ? inf->addr3n != NULL : true);

  assert ((wh->addr3 != (uint64_t)-1) ? (wh->addr2 != (uint64_t)-1) : true);
  assert ((wh->addr2 != (uint64_t)-1) ? (wh->addr1 != (uint64_t)-1) : true);

  /* GCC insists on checking format parameters and emits a warning
     when we don't use string literal.  With -Werror this ends up
     being hard error.  So instead we walk around this warning by
     using function pointer.  */
  int (*x_asprintf)(char **strp, const char *fmt, ...) = asprintf;

#define SETUP_ADDR(N)							\
  char *addr##N##s;							\
  bool free_s##N = false;						\
  if (wh->addr##N == (uint64_t)-1)					\
    addr##N##s = NULL;							\
  else if (x_asprintf (&addr##N##s, inf->addr##N##f, wh->addr##N) >= 0)	\
    free_s##N = true;							\
  else									\
    addr##N##s = "(fmt error)"

  SETUP_ADDR (1);
  SETUP_ADDR (2);
  SETUP_ADDR (3);
#undef SETUP_ADDR

  char *orig = ptr;
  bool is_reloc = wh->section == sec_rel || wh->section == sec_rela;
  if (ptr == NULL)
    {
      ptr = stpcpy (buf, inf->name);
      if (is_reloc)
	{
	  struct where const *ref = wh->ref;
	  assert (ref != NULL);
	  if (ref->section == sec_locexpr)
	    {
	      ref = ref->next;
	      assert (ref != NULL);
	      assert (ref->section != sec_locexpr);
	    }
	  ptr = stpcpy (ptr, section_names[ref->section].name);
	}

      if (addr1s != NULL)
	ptr = stpcpy (ptr, ": ");
    }

  if (addr3s != NULL)
    ptr = stpcpy (stpcpy (stpcpy (ptr, inf->addr3n), " "), addr3s);
  else if (addr2s != NULL)
    ptr = stpcpy (stpcpy (stpcpy (ptr, inf->addr2n), " "), addr2s);
  else if (addr1s != NULL)
    ptr = stpcpy (stpcpy (stpcpy (ptr, inf->addr1n), " "), addr1s);

  if (free_s1)
    free (addr1s);
  if (free_s2)
    free (addr2s);
  if (free_s3)
    free (addr3s);

  if (wh->ref != NULL && !is_reloc)
    {
      ptr = stpcpy (ptr, " (");
      ptr = (char *)where_fmt (wh->ref, ptr);
      *ptr++ = ')';
      *ptr = 0;
    }

  if (orig == NULL)
    return buf;
  else
    return ptr;
}

void
where_fmt_chain (const struct where *wh, const char *severity)
{
  if (wh != NULL && show_refs ())
    for (struct where const *it = wh->next; it != NULL; it = it->next)
      printf ("%s: %s: caused by this reference.\n",
	      severity, where_fmt (it, NULL));
}

void
where_reset_1 (struct where *wh, uint64_t addr)
{
  wh->addr1 = addr;
  wh->addr2 = wh->addr3 = (uint64_t)-1;
}

void
where_reset_2 (struct where *wh, uint64_t addr)
{
  wh->addr2 = addr;
  wh->addr3 = (uint64_t)-1;
}

void
where_reset_3 (struct where *wh, uint64_t addr)
{
  wh->addr3 = addr;
}
