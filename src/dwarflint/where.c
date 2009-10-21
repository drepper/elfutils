#include "where.h"
#include "config.h"

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
  if (wh->addr##N == (uint64_t)-1)					\
    addr##N##s = NULL;							\
  else if (x_asprintf (&addr##N##s, inf->addr##N##f, wh->addr##N) < 0)	\
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
	  struct where *ref = wh->ref;
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
  if (wh != NULL && show_refs)
    for (struct where *it = wh->next; it != NULL; it = it->next)
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
