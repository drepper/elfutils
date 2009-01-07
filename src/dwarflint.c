/* Pedantic checking of DWARF files.
   Copyright (C) 2008,2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2008.

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

#include <sys/stat.h>
#include <sys/types.h>
#include <argp.h>
#include <assert.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
#include <unistd.h>

#include "../libdw/dwarf.h"
#include "../libdw/libdwP.h"

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

#define ARGP_strict	300

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{

  { "strict", ARGP_strict, NULL, 0,
    N_("Be extremely strict, flag level 2 features."), 0 },
  { "quiet", 'q', NULL, 0, N_("Do not print anything if successful"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Pedantic checking of DWARF stored in ELF files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE...");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

static void
process_file (int fd, Dwarf *dwarf, const char *fname,
	      size_t size, bool only_one);

/* Report an error.  */
#define ERROR(str, args...)			\
  do {						\
    printf (str, ##args);			\
    ++error_count;				\
  } while (0)
static unsigned int error_count;

#define WARNING(str, args...)			\
  printf ("warning: "str, ##args)

/* True if we should perform very strict testing.  */
static bool be_strict;

/* True if no message is to be printed if the run is succesful.  */
static bool be_quiet;

int
main (int argc, char *argv[])
{
  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Initialize the message catalog.  */
  textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Before we start tell the ELF library which version we are using.  */
  elf_version (EV_CURRENT);

  /* Now process all the files given at the command line.  */
  bool only_one = remaining + 1 == argc;
  do
    {
      /* Open the file.  */
      int fd = open (argv[remaining], O_RDONLY);
      if (fd == -1)
	{
	  error (0, errno, gettext ("cannot open input file"));
	  continue;
	}

      /* Create an `Elf' descriptor.  */
      Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
      if (elf == NULL)
	ERROR (gettext ("cannot generate Elf descriptor: %s\n"),
	       elf_errmsg (-1));
      else
	{
	  unsigned int prev_error_count = error_count;
	  Dwarf *dwarf = dwarf_begin_elf (elf, DWARF_C_READ, NULL);
	  if (dwarf == NULL)
	    ERROR (gettext ("cannot generate Dwarf descriptor: %s\n"),
		   dwarf_errmsg (-1));

	  else
	    {
	      struct stat64 st;

	      if (fstat64 (fd, &st) != 0)
		{
		  printf ("cannot stat '%s': %m\n", argv[remaining]);
		  close (fd);
		  continue;
		}

	      process_file (fd, dwarf, argv[remaining], st.st_size, only_one);

	      /* Now we can close the descriptor.  */
	      if (dwarf_end (dwarf) != 0)
		ERROR (gettext ("error while closing Dwarf descriptor: %s\n"),
		       dwarf_errmsg (-1));
	    }

	  if (elf_end (elf) != 0)
	    ERROR (gettext ("error while closing Elf descriptor: %s\n"),
		   elf_errmsg (-1));

	  if (prev_error_count == error_count && !be_quiet)
	    puts (gettext ("No errors"));
	}

      close (fd);
    }
  while (++remaining < argc);

  return error_count != 0;
}

/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg __attribute__ ((unused)),
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case ARGP_strict:
      be_strict = true;
      break;

    case 'q':
      be_quiet = true;
      break;

    case ARGP_KEY_NO_ARGS:
      fputs (gettext ("Missing file name.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR,
		 program_invocation_short_name);
      exit (1);

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

#define REALLOC(A, BUF)						\
  do {								\
    if (A->size == A->alloc)					\
      {								\
	if (A->alloc == 0)					\
	  A->alloc = 8;						\
	else							\
	  A->alloc *= 2;					\
	A->BUF = xrealloc (A->BUF,				\
			   sizeof (*A->BUF) * A->alloc);	\
      }								\
  } while (0)

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64
#define PRI_ATTR "attribute 0x%" PRIx64
#define PRI_ABBR "abbrev 0x%" PRIx64
#define PRI_CU_DIE PRI_CU ", " PRI_DIE
#define PRI_CU_DIE_ABBR_ATTR PRI_CU_DIE ", " PRI_ABBR ", " PRI_ATTR
#define PRI_ABBR_ATTR PRI_ABBR ", " PRI_ATTR


/* Functions and data structures related to bounds-checked
   reading.  */

struct read_ctx
{
  Dwarf *dbg;
  Elf_Data *data;
  const unsigned char *ptr;
  const unsigned char *begin;
  const unsigned char *end;
};


static void read_ctx_init (struct read_ctx *ctx, Dwarf *dbg,
			   Elf_Data *data);
static void read_ctx_init_sub (struct read_ctx *ctx, Dwarf *dbg,
			       Elf_Data *data,
			       const unsigned char *begin,
			       const unsigned char *end);
static off64_t read_ctx_get_offset (struct read_ctx *ctx);
static bool read_ctx_need_data (struct read_ctx *ctx, size_t length);
static bool read_ctx_read_ubyte (struct read_ctx *ctx, unsigned char *ret);
static int read_ctx_read_uleb128 (struct read_ctx *ctx, uint64_t *ret);
static int read_ctx_read_sleb128 (struct read_ctx *ctx, int64_t *ret);
static bool read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret);
static bool read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret);
static bool read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret);
static bool read_ctx_read_offset (struct read_ctx *ctx, bool dwarf64,
			     uint64_t *ret);
static bool read_ctx_read_var (struct read_ctx *ctx, int width, uint64_t *ret);
static bool read_ctx_skip (struct read_ctx *ctx, uint64_t len);


/* Functions and data structures related to raw (i.e. unassisted by
   libdw) Dwarf abbreviation handling.  */

struct abbrev
{
  uint64_t code;

  /* While ULEB128 can hold numbers > 32bit, these are not legal
     values of many enum types.  So just use as large type as
     necessary to cover valid values.  */
  uint16_t tag;
  bool has_children;

  /* Whether some DIE uses this abbrev.  */
  bool used;

  /* Attributes.  */
  struct abbrev_attrib
  {
    uint64_t offset;
    uint16_t name;
    uint8_t form;
  } *attribs;
  size_t size;
  size_t alloc;
};

struct abbrev_table
{
  uint64_t offset;
  struct abbrev *abbr;
  size_t size;
  size_t alloc;
  struct abbrev_table *next;
};

static struct abbrev_table *abbrev_table_load (struct read_ctx *ctx);
static void abbrev_table_free (struct abbrev_table *abbr);
static struct abbrev *abbrev_table_find_abbrev (struct abbrev_table *abbrevs,
						uint64_t abbrev_code);


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


/* Functions and data structures for handling of address range
   coverage.  We use that to find holes of unused byts in DWARF string
   table.  */

typedef uint_fast32_t coverage_emt_type;
static const size_t coverage_emt_size = sizeof (coverage_emt_type);
static const size_t coverage_emt_bits = 8 * sizeof (coverage_emt_type);

struct coverage
{
  size_t alloc;
  uint64_t size;
  coverage_emt_type *buf;
};

static void coverage_init (struct coverage *ar, uint64_t size);
static void coverage_add (struct coverage *ar, uint64_t begin, uint64_t end);
static void coverage_find_holes (struct coverage *ar,
				 void (*cb)(uint64_t begin, uint64_t end));
static void coverage_free (struct coverage *ar);


/* Functions for checking of structural integrity.  */

static void check_debug_info_structural (struct read_ctx *ctx,
					 struct abbrev_table *abbrev_chain,
					 Elf_Data *strings);
static int read_die_chain (struct read_ctx *ctx, uint64_t cu_off,
			   struct abbrev_table *abbrevs, Elf_Data *strings,
			   bool dwarf_64, bool addr_64, int in_section,
			   struct addr_record *die_addrs,
			   struct addr_record *die_refs,
			   struct addr_record *die_loc_refs,
			   struct coverage *strings_coverage);
static void check_cu_structural (struct read_ctx *ctx, uint64_t cu_off,
				 struct abbrev_table *abbrev_chain,
				 Elf_Data *strings, bool dwarf_64,
				 bool last_section,
				 struct addr_record *die_addrs,
				 struct addr_record *die_refs,
				 struct coverage *strings_coverage);


static void
process_file (int fd __attribute__((unused)),
	      Dwarf *dwarf, const char *fname,
	      size_t size __attribute__((unused)),
	      bool only_one)
{
  if (!only_one)
    printf ("\n%s:\n", fname);

  struct read_ctx ctx;

  read_ctx_init (&ctx, dwarf, dwarf->sectiondata[IDX_debug_abbrev]);
  struct abbrev_table *abbrev_chain = abbrev_table_load (&ctx);

  read_ctx_init (&ctx, dwarf, dwarf->sectiondata[IDX_debug_info]);
  check_debug_info_structural (&ctx, abbrev_chain,
			       dwarf->sectiondata[IDX_debug_str]);

  abbrev_table_free (abbrev_chain);
}

static void
read_ctx_init (struct read_ctx *ctx, Dwarf *dbg, Elf_Data *data)
{
  if (data == NULL)
    abort ();

  read_ctx_init_sub (ctx, dbg, data, data->d_buf, data->d_buf + data->d_size);
}

static void
read_ctx_init_sub (struct read_ctx *ctx, Dwarf *dbg, Elf_Data *data,
		   const unsigned char *begin, const unsigned char *end)
{
  if (data == NULL)
    abort ();

  ctx->dbg = dbg;
  ctx->data = data;
  ctx->begin = begin;
  ctx->end = end;
  ctx->ptr = begin;
}

static off64_t
read_ctx_get_offset (struct read_ctx *ctx)
{
  return ctx->ptr - ctx->begin;
}

static bool
read_ctx_need_data (struct read_ctx *ctx, size_t length)
{
  const unsigned char *ptr = ctx->ptr + length;
  return ptr <= ctx->end && (length == 0 || ptr > ctx->ptr);
}

static bool
read_ctx_read_ubyte (struct read_ctx *ctx, unsigned char *ret)
{
  if (!read_ctx_need_data (ctx, 1))
    return false;
  *ret = *ctx->ptr++;
  return true;
}

static int
read_ctx_read_uleb128 (struct read_ctx *ctx, uint64_t *ret)
{
  uint64_t result = 0;
  int shift = 0;
  int size = 8 * sizeof (result);
  bool zero_tail = false;

  while (1)
    {
      uint8_t byte;
      if (!read_ctx_read_ubyte (ctx, &byte))
	return -1;

      uint8_t payload = byte & 0x7f;
      zero_tail = payload == 0 && shift > 0;
      result |= (uint64_t)payload << shift;
      shift += 7;
      if (shift > size)
	return -1;
      if ((byte & 0x80) == 0)
	break;
    }

  *ret = result;
  return zero_tail ? 1 : 0;
}

#define CHECKED_READ_ULEB128(CTX, RET, FMT, WHAT, ARGS...)		\
  ({									\
      int __st = read_ctx_read_uleb128 (CTX, RET);			\
      bool __ret = false;						\
      if (__st < 0)							\
	ERROR (FMT ": can't read %s.\n", ##ARGS, WHAT);			\
      else if (__st > 0)						\
	WARNING (FMT ": unnecessarily long encoding of %s.\n", ##ARGS, WHAT); \
      else								\
	__ret = true;							\
      __ret;								\
  })

static int
read_ctx_read_sleb128 (struct read_ctx *ctx, int64_t *ret)
{
  int64_t result = 0;
  int shift = 0;
  int size = 8 * sizeof (result);
  bool zero_tail = false;
  bool sign = false;

  while (1)
    {
      uint8_t byte;
      if (!read_ctx_read_ubyte (ctx, &byte))
	return -1;

      uint8_t payload = byte & 0x7f;
      zero_tail = shift > 0 && ((payload == 0x7f && sign)
				|| (payload == 0 && !sign));
      sign = (byte & 0x40) != 0; /* Set sign for rest of loop & next round.  */
      result |= (int64_t)payload << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	{
	  if (shift < size && sign)
	    result |= -((int64_t)1 << shift);
	  break;
	}
      if (shift > size)
	return -1;
    }

  *ret = result;
  return zero_tail ? 1 : 0;
}

#define CHECKED_READ_SLEB128(CTX, RET, FMT, WHAT, ARGS...)		\
  ({									\
      int __st = read_ctx_read_sleb128 (CTX, RET);			\
      bool __ret = true;						\
      if (__st < 0)							\
	{								\
	  ERROR (FMT ": can't read %s.\n", ##ARGS, WHAT);		\
	  __ret = false;						\
	}								\
      else if (__st > 0)						\
	WARNING (FMT ": unnecessarily long encoding of %s.\n",		\
		 ##ARGS, WHAT);						\
      __ret;								\
  })

static bool
read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret)
{
  if (!read_ctx_need_data (ctx, 2))
    return false;
  *ret = read_2ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  return true;
}

static bool
read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret)
{
  if (!read_ctx_need_data (ctx, 4))
    return false;
  *ret = read_4ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  return true;
}

static bool
read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret)
{
  if (!read_ctx_need_data (ctx, 8))
    return false;
  *ret = read_8ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  return true;
}

static bool
read_ctx_read_offset (struct read_ctx *ctx, bool dwarf64, uint64_t *ret)
{
  if (dwarf64)
    return read_ctx_read_8ubyte (ctx, ret);

  uint32_t v;
  if (!read_ctx_read_4ubyte (ctx, &v))
    return false;

  *ret = v;
  return true;
}

static bool
read_ctx_read_var (struct read_ctx *ctx, int width, uint64_t *ret)
{
  switch (width)
    {
    case 4:
    case 8:
      return read_ctx_read_offset (ctx, width == 8, ret);
    case 2:
      {
	uint16_t val;
	if (!read_ctx_read_2ubyte (ctx, &val))
	  return false;
	*ret = val;
	return true;
      }
    case 1:
      {
	uint8_t val;
	if (!read_ctx_read_ubyte (ctx, &val))
	  return false;
	*ret = val;
	return true;
      }
    default:
      return false;
    };
}

static bool
read_ctx_skip (struct read_ctx *ctx, uint64_t len)
{
  if (!read_ctx_need_data (ctx, len))
    return false;
  ctx->ptr += len;
  return true;
}

static bool
attrib_form_valid (uint64_t form)
{
  return form > 0 && form <= DW_FORM_indirect;
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

static struct abbrev_table *
abbrev_table_load (struct read_ctx *ctx)
{
  struct abbrev_table *section_chain = NULL;
  struct abbrev_table *section = NULL;

#define SECTION_ENDS (ctx->ptr >= ctx->end)

  while (!SECTION_ENDS)
    {
      uint64_t abbr_off, prev_abbr_off = (uint64_t)-1;
      uint64_t abbr_code, prev_abbr_code = (uint64_t)-1;
      uint64_t abbr_tag;
      uint64_t zero_seq_off = (uint64_t)-1;

      while (!SECTION_ENDS)
	{
	  abbr_off = read_ctx_get_offset (ctx);

	  /* Abbreviation code.  */
	  if (!CHECKED_READ_ULEB128 (ctx, &abbr_code,
				     PRI_ABBR, "abbrev code", abbr_off))
	    goto free_and_out;

	  if (abbr_code == 0 && prev_abbr_code == 0
	      && zero_seq_off == (uint64_t)-1)
	    zero_seq_off = prev_abbr_off;

	  if (abbr_code != 0)
	    break;
	  else
	    section = NULL;

	  prev_abbr_code = abbr_code;
	  prev_abbr_off = abbr_off;
	}

      if (zero_seq_off != (uint64_t)-1)
	WARNING (PRI_ABBR": unnecessary padding with zero bytes.\n", zero_seq_off);

      if (SECTION_ENDS)
	break;

#undef SECTION_ENDS

      if (section == NULL)
	{
	  section = xcalloc (1, sizeof (*section));
	  section->offset = abbr_off;
	  section->next = section_chain;
	  section_chain = section;
	}
      REALLOC (section, abbr);

      struct abbrev *cur = section->abbr + section->size++;
      memset (cur, 0, sizeof (*cur));

      cur->code = abbr_code;

      /* Abbreviation tag.  */
      if (!CHECKED_READ_ULEB128 (ctx, &abbr_tag,
				 PRI_ABBR, "abbrev tag", abbr_off))
	goto free_and_out;

      if (abbr_tag > DW_TAG_hi_user)
	{
	  ERROR (PRI_ABBR ": invalid abbrev tag 0x%" PRIx64 ".\n",
		 abbr_off, abbr_tag);
	  goto free_and_out;
	}
      cur->tag = (typeof (cur->tag))abbr_tag;

      /* Abbreviation has_children.  */
      uint8_t has_children;
      if (!read_ctx_read_ubyte (ctx, &has_children))
	{
	  ERROR (PRI_ABBR ": can't read abbrev has_children.\n", abbr_off);
	  goto free_and_out;
	}

      if (has_children != DW_CHILDREN_no
	  && has_children != DW_CHILDREN_yes)
	{
	  ERROR (PRI_ABBR ": invalid has_children value 0x%x.\n",
		 abbr_off, cur->has_children);
	  goto free_and_out;
	}
      cur->has_children = has_children == DW_CHILDREN_yes;

      bool null_attrib;
      uint64_t sibling_attr = 0;
      do
	{
	  uint64_t attr_off = read_ctx_get_offset (ctx);
	  uint64_t attrib_name, attrib_form;

	  /* Load attribute name and form.  */
	  if (!CHECKED_READ_ULEB128 (ctx, &attrib_name,
				     PRI_ABBR_ATTR, "attribute name",
				     abbr_off, attr_off))
	    goto free_and_out;

	  if (!CHECKED_READ_ULEB128 (ctx, &attrib_form,
				     PRI_ABBR_ATTR, "attribute form",
				     abbr_off, attr_off))
	    goto free_and_out;

	  null_attrib = attrib_name == 0 && attrib_form == 0;

	  /* Now if both are zero, this was the last attribute.  */
	  if (!null_attrib)
	    {
	      /* Otherwise validate name and form.  */
	      if (attrib_name > DW_AT_hi_user)
		{
		  ERROR (PRI_ABBR_ATTR ": invalid name 0x%" PRIx64 ".\n",
			 abbr_off, attr_off, attrib_name);
		  goto free_and_out;
		}

	      if (!attrib_form_valid (attrib_form))
		{
		  ERROR (PRI_ABBR_ATTR ": invalid form 0x%" PRIx64 ".\n",
			 abbr_off, attr_off, attrib_form);
		  goto free_and_out;
		}
	    }

	  REALLOC (cur, attribs);

	  struct abbrev_attrib *acur = cur->attribs + cur->size++;
	  memset (acur, 0, sizeof (*acur));

	  /* We do structural checking of sibling attribute, so make
	     sure our assumptions in actual DIE-loading code are
	     right.  We expect at most one DW_AT_sibling attribute,
	     with form from reference class, but only CU-local, not
	     DW_FORM_ref_addr.  */
	  if (attrib_name == DW_AT_sibling)
	    {
	      if (sibling_attr != 0)
		ERROR (PRI_ABBR_ATTR
		       ": Another DW_AT_sibling attribute in one abbreviation. "
		       "(First was 0x%" PRIx64 ".)\n",
		       abbr_off, attr_off, sibling_attr);
	      else
		{
		  assert (attr_off > 0);
		  sibling_attr = attr_off;

		  if (!cur->has_children)
		    WARNING (PRI_ABBR_ATTR
			     ": Excessive DW_AT_sibling attribute at childless abbrev.\n",
			     abbr_off, attr_off);
		}

	      switch (check_sibling_form (attrib_form))
		{
		case -1:
		  WARNING (PRI_ABBR_ATTR
			   ": DW_AT_sibling attribute with form DW_FORM_ref_addr.\n",
			   abbr_off, attr_off);
		  break;

		case -2:
		  ERROR (PRI_ABBR_ATTR
			 ": DW_AT_sibling attribute with non-reference form 0x%" PRIx64 ".\n",
			 abbr_off, attr_off, attrib_form);
		};
	    }

	  acur->name = attrib_name;
	  acur->form = attrib_form;
	  acur->offset = attr_off;
	}
      while (!null_attrib);
    }

  return section_chain;

 free_and_out:
  abbrev_table_free (section_chain);
  return NULL;
}

static void
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

static struct abbrev *
abbrev_table_find_abbrev (struct abbrev_table *abbrevs, uint64_t abbrev_code)
{
  for (size_t i = 0; i < abbrevs->size; ++i)
    if (abbrevs->abbr[i].code == abbrev_code)
      return abbrevs->abbr + i;
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
  free (ar->addrs);
}

static void
coverage_init (struct coverage *ar, uint64_t size)
{
  size_t ctemts = size / coverage_emt_bits + 1;
  ar->buf = xcalloc (ctemts, sizeof (ar->buf));
  ar->alloc = ctemts;
  ar->size = size;
}

static void
coverage_add (struct coverage *ar, uint64_t begin, uint64_t end)
{
  assert (begin <= end);
  assert (end <= ar->size);

  uint64_t bi = begin / coverage_emt_bits;
  uint64_t ei = end / coverage_emt_bits;

  uint8_t bb = begin % coverage_emt_bits;
  uint8_t eb = end % coverage_emt_bits;

  coverage_emt_type bm = (coverage_emt_type)-1 >> bb;
  coverage_emt_type em = (coverage_emt_type)-1 << (coverage_emt_bits - 1 - eb);

  if (bi == ei)
    ar->buf[bi] |= bm & em;
  else
    {
      ar->buf[bi] |= bm;
      ar->buf[ei] |= em;
      memset (ar->buf + bi + 1, -1, coverage_emt_size * (ei - bi - 1));
    }
}

static void
coverage_find_holes (struct coverage *ar,
		     void (*cb)(uint64_t begin, uint64_t end))
{
  bool hole;
  uint64_t begin = 0;

  void hole_begin (uint64_t a)
  {
    begin = a;
    hole = true;
  }

  void hole_end (uint64_t a)
  {
    assert (hole);
    if (a != begin)
      cb (begin, a - 1);
    hole = false;
  }

  hole_begin (0);
  for (size_t i = 0; i < ar->alloc; ++i)
    {
      if (ar->buf[i] == (coverage_emt_type)-1)
	{
	  if (hole)
	    hole_end (i * coverage_emt_bits);
	}
      else
	{
	  coverage_emt_type tmp = ar->buf[i];
	  for (uint8_t j = 1; j <= coverage_emt_bits; ++j)
	    {
	      coverage_emt_type mask
		= (coverage_emt_type)1 << (coverage_emt_bits - j);
	      uint64_t addr = i * coverage_emt_bits + j - 1;
	      if (addr > ar->size)
		break;
	      if (!hole && !(tmp & mask))
		hole_begin (addr);
	      else if (hole && (tmp & mask))
		hole_end (addr);
	    }
	}
    }
  if (hole)
    hole_end (ar->size);
}

static void
coverage_free (struct coverage *ar)
{
  free (ar->buf);
}

static void
check_addr_record_addr (struct addr_record *ar, uint64_t addr)
{
  if (!addr_record_has_addr (ar, addr))
    ERROR ("Unresolved DIE reference to " PRI_DIE ".\n", addr);
}

static void
check_die_references (struct addr_record *die_addrs,
		      struct addr_record *die_refs)
{
  for (size_t i = 0; i < die_refs->size; )
    {
      uint64_t addr = die_refs->addrs[i];
      check_addr_record_addr (die_addrs, addr);

      for (; i < die_refs->size; ++i)
	if (die_refs->addrs[i] != addr)
	  break;
    }
}

static void
check_debug_info_structural (struct read_ctx *ctx,
			     struct abbrev_table *abbrev_chain,
			     Elf_Data *strings)
{
  struct addr_record die_addrs_mem;
  struct addr_record *die_addrs = &die_addrs_mem;
  memset (die_addrs, 0, sizeof (*die_addrs));

  struct addr_record die_refs_mem;
  struct addr_record *die_refs = &die_refs_mem;
  memset (die_refs, 0, sizeof (*die_refs));

  bool recording = true;

  struct coverage strings_coverage_mem;
  struct coverage *strings_coverage = NULL;
  if (be_strict)
    {
      coverage_init (&strings_coverage_mem, strings->d_size);
      strings_coverage = &strings_coverage_mem;
    }

  while (ctx->ptr < ctx->end)
    {
      const unsigned char *cu_begin = ctx->ptr;
      uint64_t cu_off = read_ctx_get_offset (ctx);
      uint32_t size32;
      uint64_t size;
      bool dwarf_64 = false;

      /* CU length.  */
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  ERROR (PRI_CU ": can't read CU length.\n", cu_off);
	  return;
	}
      if (size32 != DWARF3_LENGTH_64_BIT)
	size = size32;
      else
	{
	  if (!read_ctx_read_8ubyte (ctx, &size))
	    {
	      ERROR (PRI_CU ": can't read 64bit CU length.\n", cu_off);
	      return;
	    }

	  dwarf_64 = true;
	}

      /* Make CU context begin just before the CU length, so that DIE
	 offsets are computed correctly.  */
      struct read_ctx cu_ctx;
      const unsigned char *cu_end = ctx->ptr + size;
      read_ctx_init_sub (&cu_ctx, ctx->dbg, ctx->data, cu_begin, cu_end);
      cu_ctx.ptr = ctx->ptr;

      check_cu_structural (&cu_ctx, cu_off, abbrev_chain, strings,
			   dwarf_64, cu_end == ctx->end,
			   die_addrs, die_refs, strings_coverage);
      ctx->ptr += size;
    }

  if (ctx->ptr != ctx->end)
    ERROR ("Suspicious: CU lengths don't exactly match Elf_Data contents.");

  if (recording)
    {
      check_die_references (die_addrs, die_refs);
      addr_record_free (&die_addrs_mem);
      addr_record_free (&die_refs_mem);
    }

  if (strings_coverage != NULL)
    {
      void hole (uint64_t begin, uint64_t end)
      {
	/* XXX only report holes of non-zero bytes.  Be quiet about
	   zero bytes that seem to be present for alignment purposes.
	   Check that zeroes are not excessive, i.e. it's not possible
	   to achieve that alignment with fewer zero bytes.  */
	WARNING ("Unreferenced portion of .debug_str: "
		 "0x%" PRIx64 "..0x%" PRIx64 ".\n",
		 begin, end);
      }

      coverage_find_holes (strings_coverage, hole);
      coverage_free (strings_coverage);
    }
}


/*
  Returns:
    -1 in case of error
    +0 in case of no error, but the chain only consisted of a
       terminating zero die.
    +1 in case some dies were actually loaded

  SECTION parameter:
    +0 not checking a section chain (NUL die termination required)
    +1 reading a section chain (only one DIE is allowed, NUL die
       termination is excessive)
    +2 reading a section chain of last section (one DIE, but NUL die
       termination is OK if done for padding purposes)
 */
static int
read_die_chain (struct read_ctx *ctx, uint64_t cu_off,
		struct abbrev_table *abbrevs, Elf_Data *strings,
		bool dwarf_64, bool addr_64, int in_section,
		struct addr_record *die_addrs,
		struct addr_record *die_refs,
		struct addr_record *die_loc_refs,
		struct coverage *strings_coverage)
{
  bool got_null = false;
  bool got_die = false;
  const unsigned char *begin = ctx->ptr;
  uint64_t sibling_addr = 0;
  uint64_t die_off, prev_die_off = 0;
  struct abbrev *abbrev, *prev_abbrev = NULL;

  while (ctx->ptr < ctx->end)
    {
      uint64_t abbrev_code;
      die_off = read_ctx_get_offset (ctx);

      if (sibling_addr != 0)
	{
	  if (sibling_addr != die_off)
	    ERROR (PRI_CU_DIE
		   ": This DIE should have had its sibling at 0x%"
		   PRIx64 ", but it's at 0x%" PRIx64 " instead.\n",
		   cu_off, prev_die_off, sibling_addr, die_off);
	  sibling_addr = 0;
	}
      else if (prev_abbrev != NULL && prev_abbrev->has_children)
	ERROR (PRI_CU_DIE
	       ": This DIE had children, but no DW_AT_sibling attribute.\n",
	       cu_off, prev_die_off);

      prev_die_off = die_off;

      /* Abbrev code.  */
      if (!CHECKED_READ_ULEB128 (ctx, &abbrev_code,
				 PRI_CU_DIE, "abbrev code",
				 cu_off, die_off))
	return -1;

      if (abbrev_code == 0)
	{
	  got_null = true;
	  if (in_section != 2)
	    break;
	  else
	    continue;
	}
      else if (got_null)
	ERROR (PRI_CU_DIE
	       ": invalid DIE with non-zero abbrev code after sequence "
	       "of DIEs with abbrev code 0.\n",
	       cu_off, die_off);

      got_die = true;

      abbrev = abbrev_table_find_abbrev (abbrevs, abbrev_code);
      if (abbrev == NULL)
	{
	  ERROR (PRI_CU_DIE ": abbrev section at 0x%" PRIx64
		 " doesn't contain code %" PRIu64 ".\n",
		 cu_off, die_off, abbrevs->offset, abbrev_code);
	  return -1;
	}
      abbrev->used = true;

      if (die_addrs != NULL)
	addr_record_add (die_addrs, cu_off + die_off);

      /* Attribute values.  */
      for (struct abbrev_attrib *it = abbrev->attribs;
	   it->name != 0; ++it)
	{

	  void record_ref (uint64_t addr, bool local)
	  {
	    struct addr_record *record = die_refs;
	    if (local)
	      {
		assert (ctx->end > ctx->begin);
		if (addr > (uint64_t)(ctx->end - ctx->begin))
		  {
		    ERROR (PRI_CU_DIE_ABBR_ATTR
			   ": Invalid reference outside the CU: 0x%" PRIx64 ".\n",
			   cu_off, die_off, abbrev->code, it->offset, addr);
		    return;
		  }

		addr += cu_off;
		record = die_loc_refs;
	      }

	    if (die_refs != NULL)
	      addr_record_add (record, addr);
	  }

	  uint8_t form = it->form;
	  if (form == DW_FORM_indirect)
	    {
	      uint64_t value;
	      if (!CHECKED_READ_ULEB128 (ctx, &value, PRI_CU_DIE_ABBR_ATTR,
					 "indirect attribute form",
					 cu_off, die_off, abbrev->code,
					 it->offset))
		return -1;

	      if (!attrib_form_valid (value))
		{
		  ERROR (PRI_CU_DIE_ABBR_ATTR
			 ": invalid indirect form 0x%" PRIx64 ".\n",
			 cu_off, die_off, abbrev->code, it->offset, value);
		  return -1;
		}
	      form = value;

	      if (it->name == DW_AT_sibling)
		switch (check_sibling_form (form))
		  {
		  case -1:
		    WARNING (PRI_CU_DIE_ABBR_ATTR
			     ": DW_AT_sibling attribute with (indirect) form DW_FORM_ref_addr.\n",
			     cu_off, die_off, abbrev->code, it->offset);
		    break;

		  case -2:
		    /* XXX write form name  */
		    ERROR (PRI_CU_DIE_ABBR_ATTR
			   ": DW_AT_sibling attribute with non-reference (indirect) form 0x%" PRIx64 ".\n",
			   cu_off, die_off, abbrev->code, it->offset, value);
		  };
	    }

	  switch (form)
	    {
	    case DW_FORM_strp:
	      {
		uint64_t addr;
		if (!read_ctx_read_offset (ctx, dwarf_64, &addr))
		  {
		  cant_read:
		    ERROR (PRI_CU_DIE_ABBR_ATTR
			   ": can't read attribute value.\n",
			   cu_off, die_off, abbrev->code, it->offset);
		    return -1;
		  }

		if (strings == NULL)
		  ERROR (PRI_CU_DIE_ABBR_ATTR
			 ": strp attribute, but no .debug_str section.\n",
			 cu_off, die_off, abbrev->code, it->offset);
		else if (addr >= strings->d_size)
		  ERROR (PRI_CU_DIE_ABBR_ATTR
			 ": Invalid offset outside .debug_str: 0x%" PRIx64 ".",
			 cu_off, die_off, abbrev->code, it->offset, addr);
		else
		  {
		    /* Record used part of .debug_str.  */
		    const char *strp = (const char *)strings->d_buf + addr;
		    uint64_t end = addr + strlen (strp);

		    if (strings_coverage != NULL)
		      coverage_add (strings_coverage, addr, end);
		  }

		break;
	      }

	    case DW_FORM_string:
	      {
		/* XXX check encoding? DW_AT_use_UTF8 */
		uint8_t byte;
		do
		  {
		    if (!read_ctx_read_ubyte (ctx, &byte))
		      goto cant_read;
		  }
		while (byte != 0);
		break;
	      }

	    case DW_FORM_addr:
	    case DW_FORM_ref_addr:
	      {
		uint64_t addr;
		if (!read_ctx_read_offset (ctx, addr_64, &addr))
		  goto cant_read;

		if (it->form == DW_FORM_ref_addr)
		  record_ref (addr, false);

		/* XXX What are validity criteria for DW_FORM_addr? */
		break;
	      }

	    case DW_FORM_udata:
	    case DW_FORM_ref_udata:
	      {
		uint64_t value;
		if (!CHECKED_READ_ULEB128 (ctx, &value, PRI_CU_DIE_ABBR_ATTR,
					   "attribute value",
					   cu_off, die_off, abbrev->code,
					   it->offset))
		  return -1;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref_udata)
		  record_ref (value, true);
		break;
	      }

	    case DW_FORM_flag:
	    case DW_FORM_data1:
	    case DW_FORM_ref1:
	      {
		uint8_t value;
		if (!read_ctx_read_ubyte (ctx, &value))
		  goto cant_read;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref1)
		  record_ref (value, true);
		break;
	      }

	    case DW_FORM_data2:
	    case DW_FORM_ref2:
	      {
		uint16_t value;
		if (!read_ctx_read_2ubyte (ctx, &value))
		  goto cant_read;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref2)
		  record_ref (value, true);
		break;
	      }

	    case DW_FORM_data4:
	    case DW_FORM_ref4:
	      {
		uint32_t value;
		if (!read_ctx_read_4ubyte (ctx, &value))
		  goto cant_read;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref4)
		  record_ref (value, true);
		break;
	      }

	    case DW_FORM_data8:
	    case DW_FORM_ref8:
	      {
		uint64_t value;
		if (!read_ctx_read_8ubyte (ctx, &value))
		  goto cant_read;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref8)
		  record_ref (value, true);
		break;
	      }

	    case DW_FORM_sdata:
	      {
		int64_t value;
		if (!CHECKED_READ_SLEB128 (ctx, &value, PRI_CU_DIE_ABBR_ATTR,
					   "attribute value",
					   cu_off, die_off, abbrev->code,
					   it->offset))
		  return -1;
		break;
	      }

	    case DW_FORM_block:
	      {
		int width = 0;
		uint64_t length;
		goto process_DW_FORM_block;

	      case DW_FORM_block1:
		width = 1;
		goto process_DW_FORM_block;

	      case DW_FORM_block2:
		width = 2;
		goto process_DW_FORM_block;

	      case DW_FORM_block4:
		width = 4;

	      process_DW_FORM_block:
		if (width == 0)
		  {
		    if (!CHECKED_READ_ULEB128 (ctx, &length, PRI_CU_DIE_ABBR_ATTR,
					       "attribute value",
					       cu_off, die_off, abbrev->code,
					       it->offset))
		      return -1;
		  }
		else if (!read_ctx_read_var (ctx, width, &length))
		  goto cant_read;

		if (!read_ctx_skip (ctx, length))
		  goto cant_read;

		break;
	      }

	    case DW_FORM_indirect:
	      ERROR (PRI_CU_DIE_ABBR_ATTR
		     ": Indirect form is again indirect.\n",
		     cu_off, die_off, abbrev->code, it->offset);
	      return -1;

	    default:
	      ERROR (PRI_CU_DIE_ABBR_ATTR
		     ": Internal error: unhandled form 0x%x\n",
		     cu_off, die_off, abbrev->code, it->offset, it->form);
	    }
	}

      if (abbrev->has_children)
	{
	  int st = read_die_chain (ctx, cu_off, abbrevs, strings,
				   dwarf_64, addr_64, 0,
				   die_addrs, die_refs, die_loc_refs,
				   strings_coverage);
	  if (st == -1)
	    return -1;
	  else if (st == 0 && be_strict)
	    WARNING (PRI_CU_DIE
		     ": Abbrev has_children, but the chain was empty.\n",
		     cu_off, die_off);
	}
    }

  if (sibling_addr != 0)
    ERROR (PRI_CU_DIE
	   ": This DIE should have had its sibling at 0x%"
	   PRIx64 ", but the DIE chain ended.\n",
	   cu_off, prev_die_off, sibling_addr);

  /* DIEs with zero abbreviation code are excessive if: we check
     non-final section, or we check final section and these DIEs are
     not used for alignment purposes.  */
  if (!got_null && !in_section)
    ERROR (PRI_CU
	   ": Sequence of DIEs at %p not terminated with DIE with zero abbrev code.\n",
	   cu_off, begin);
  else if (got_null && in_section == 2)
    {
      /* XXX Check that zero-abbrev DIEs are not excessive, i.e. if
	 it's not possible to achieve that alignment with fewer DIEs
	 with zero abbrev code.  */
      if (ctx->data->d_align < 2
	  || ((uintptr_t)ctx->ptr & -ctx->data->d_align) != 0)
	goto excessive;
    }
  else if (got_null && in_section == 1)
  excessive:
    WARNING (PRI_CU
	     ": Sequence of DIEs at %p unnecessarily terminated with NUL die.\n",
	     cu_off, begin);

  return got_die ? 1 : 0;
}

static void
check_cu_structural (struct read_ctx *ctx, uint64_t cu_off,
		     struct abbrev_table *abbrev_chain,
		     Elf_Data *strings, bool dwarf_64, bool last_section,
		     struct addr_record *die_addrs,
		     struct addr_record *die_refs,
		     struct coverage *strings_coverage)
{
  uint16_t version;
  uint64_t abbrev_offset;
  uint8_t address_size;

  /* CU version.  */
  if (!read_ctx_read_2ubyte (ctx, &version))
    {
      ERROR (PRI_CU ": can't read version.\n", cu_off);
      return;
    }

  if (version < 2 || version > 3)
    {
      ERROR (PRI_CU ": %s version %d.\n",
	     cu_off, (version < 2 ? "Invalid" : "Unsupported"), version);
      return;
    }

  if (version == 2 && dwarf_64)
    ERROR (PRI_CU ": Invalid 64-bit CU in DWARF 2 format.\n", cu_off);

  /* Abbrev offset.  */
  if (!read_ctx_read_offset (ctx, dwarf_64, &abbrev_offset))
    {
      ERROR (PRI_CU ": can't read abbrev offset.\n", cu_off);
      return;
    }

  /* Address size.  */
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      ERROR (PRI_CU ": can't read address size.\n", cu_off);
      return;
    }
  if (address_size != 4 && address_size != 8)
    {
      ERROR (PRI_CU ": Invalid address size: %d (only 4 or 8 allowed).\n",
	     cu_off, address_size);
      return;
    }

  struct abbrev_table *abbrevs = abbrev_chain;
  for (; abbrevs != NULL; abbrevs = abbrevs->next)
    if (abbrevs->offset == abbrev_offset)
      break;

  if (abbrevs == NULL)
    {
      ERROR (PRI_CU
	     ": Couldn't find abbrev section with offset 0x%" PRIx64 ".\n",
	     cu_off, abbrev_offset);
      return;
    }

  struct addr_record die_loc_refs_mem;
  struct addr_record *die_loc_refs = NULL;
  if (die_addrs != NULL)
    {
      die_loc_refs = &die_loc_refs_mem;
      memset (die_loc_refs, 0, sizeof (*die_loc_refs));
    }

  if (read_die_chain (ctx, cu_off, abbrevs, strings,
		      dwarf_64, address_size == 8, last_section ? 2 : 1,
		      die_addrs, die_refs, die_loc_refs,
		      strings_coverage) >= 0)
    {
      for (size_t i = 0; i < abbrevs->size; ++i)
	if (!abbrevs->abbr[i].used)
	  ERROR (PRI_CU ": Abbreviation with code %" PRIu64 " is never used.\n",
		 cu_off, abbrevs->abbr[i].code);

      check_die_references (die_addrs, die_loc_refs);
    }

  addr_record_free (&die_loc_refs_mem);
}
