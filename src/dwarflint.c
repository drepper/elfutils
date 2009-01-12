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
#include "dwarfstrings.h"

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

#define ARGP_strict	300
#define ARGP_gnu	301

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{

  { "strict", ARGP_strict, NULL, 0,
    N_("Be extremely strict, flag level 2 features."), 0 },
  { "quiet", 'q', NULL, 0, N_("Do not print anything if successful"), 0 },
  { "ignore-missing", 'i', NULL, 0,
    N_("Don't complain if files have no DWARF at all"), 0 },
  { "gnu", ARGP_gnu, NULL, 0,
    N_("Binary has been created with GNU toolchain and is therefore known to be \
broken in certain ways"), 0 },
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

/* If true, we accept silently files without debuginfo.  */
static bool tolerate_nodebug = false;

static void
process_file (int fd, Dwarf *dwarf, const char *fname,
	      size_t size, bool only_one);

enum message_category
{
  mc_none      = 0,

  /* Severity: */
  mc_impact_1  = 0x0001, // no impact on the consumer
  mc_impact_2  = 0x0002, // still no impact, but suspicious or worth mentioning
  mc_impact_3  = 0x0004, // some impact
  mc_impact_4  = 0x0008, // high impact
  mc_impact_all= 0x000f, // all severity levels
  mc_impact_2p = 0x000e, // 2+
  mc_impact_3p = 0x000c, // 3+

  /* Accuracy:  */
  mc_acc_bloat     = 0x0010, // unnecessary constructs (e.g. unreferenced strings)
  mc_acc_suboptimal= 0x0020, // suboptimal construct (e.g. lack of siblings)
  mc_acc_all       = 0x00f0, // all accuracy options

  /* Area: */
  mc_leb128    = 0x00100, // ULEB/SLEB storage
  mc_abbrevs   = 0x00200, // abbreviations and abbreviation tables
  mc_die_siblings = 0x00400, // DIE sibling relationship
  mc_die_children = 0x00800, // DIE parent/child relationship
  mc_die_other = 0x01000, // messages related to DIEs and .debug_info tables, but not covered above
  mc_die_all   = 0x01c00, // includes all above DIE categories
  mc_strings   = 0x02000, // string table
  mc_aranges   = 0x04000, // address ranges table
  mc_other     = 0x80000, // messages unrelated to any of the above
  mc_all       = 0xfff00, // all areas
};

struct message_criteria
{
  enum message_category accept; /* cat & accept must be != 0  */
  enum message_category reject; /* cat & reject must be == 0  */
};

static bool
accept_message (struct message_criteria *crit, enum message_category cat)
{
  assert (crit != NULL);
  return (crit->accept & cat) != 0
    && (crit->reject & cat) == 0;
}

static struct message_criteria warning_criteria = {mc_all & ~mc_strings, mc_none};
static struct message_criteria error_criteria = {mc_impact_4, mc_none};

static bool
check_category (enum message_category cat)
{
  return accept_message (&warning_criteria, cat);
}

/* Report an error.  */
#define ERROR(str, args...)			\
  do {						\
    fputs ("error: ", stdout);			\
    printf (str, ##args);			\
    ++error_count;				\
  } while (0)
static unsigned int error_count;

#define WARNING(str, args...)			\
  do {						\
    fputs ("warning: ", stdout);		\
    printf (str, ##args);			\
    ++error_count;				\
  } while (0)

#define MESSAGE(category, str, args...)			   \
  do {							   \
    if (accept_message (&warning_criteria, category))	   \
      {							   \
	if (accept_message (&error_criteria, category))	   \
	  ERROR (str, ##args);				   \
	else						   \
	  WARNING (str, ##args);			   \
      }							   \
  } while (0)

/* mc_acc_bloat | mc_impact_1 is automatically attached.  */
#define MESSAGE_PADDING_0(CAT, FMT, START, END, ARGS...)	\
  MESSAGE (((CAT) | mc_acc_bloat | mc_impact_1),		\
	   FMT ": 0x%" PRIx64 "..0x%" PRIx64			\
	   ": unnecessary padding with zero bytes.\n",		\
	   ##ARGS, (START), (END))

/* mc_acc_bloat | mc_impact_2 is automatically attached.  */
#define MESSAGE_PADDING_N0(CAT, FMT, START, END, ARGS...)	\
  MESSAGE (((CAT) | mc_acc_bloat | mc_impact_2),		\
	   FMT ": 0x%" PRIx64 "..0x%" PRIx64			\
	   ": unreferenced non-zero bytes.\n",			\
	   ##ARGS, (START), (END))


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
	    {
	      if (!tolerate_nodebug)
		ERROR (gettext ("cannot generate Dwarf descriptor: %s\n"),
		       dwarf_errmsg (-1));
	    }
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
      warning_criteria.accept |= mc_strings;
      break;

    case ARGP_gnu:
      warning_criteria.reject |= mc_acc_bloat;
      break;

    case 'i':
      tolerate_nodebug = true;
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

#define PRI_D_INFO ".debug_info: "
#define PRI_D_ABBREV ".debug_abbrev: "
#define PRI_D_ARANGES ".debug_aranges: "

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64
#define PRI_ATTR "attribute 0x%" PRIx64
#define PRI_ABBR "abbrev 0x%" PRIx64
#define PRI_ARANGETAB "arange table 0x%" PRIx64
#define PRI_RECORD "record 0x%" PRIx64

#define PRI_CU_DIE PRI_CU ", " PRI_DIE
#define PRI_CU_DIE_ABBR_ATTR PRI_CU_DIE ", " PRI_ABBR ", " PRI_ATTR
#define PRI_ABBR_ATTR PRI_ABBR ", " PRI_ATTR
#define PRI_ARANGETAB_CU PRI_ARANGETAB " (for " PRI_CU ")"
#define PRI_ARANGETAB_CU_RECORD PRI_ARANGETAB " (for " PRI_CU "), " PRI_RECORD


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
static bool read_ctx_eof (struct read_ctx *ctx);


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
   coverage.  We use that to find holes of unused bytes in DWARF string
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


/* Functions and data structures for CU handling.  */

struct cu
{
  uint64_t offset;
  struct addr_record die_addrs; // Addresses where DIEs begin in this CU.
  struct addr_record die_refs;  // DIE references into other CUs from this CU.
  struct cu *next;
};

static void cu_free (struct cu *cu_chain);
static struct cu *cu_find_cu (struct cu *cu_chain, uint64_t offset);


/* Functions for checking of structural integrity.  */

static struct cu *check_debug_info_structural (struct read_ctx *ctx,
					       struct abbrev_table *abbrev_chain,
					       Elf_Data *strings);
static int read_die_chain (struct read_ctx *ctx,
			   struct cu *cu,
			   struct abbrev_table *abbrevs, Elf_Data *strings,
			   bool dwarf_64, bool addr_64,
			   struct addr_record *die_refs,
			   struct addr_record *die_loc_refs,
			   struct coverage *strings_coverage);
static bool check_cu_structural (struct read_ctx *ctx,
				 struct cu *const cu,
				 struct abbrev_table *abbrev_chain,
				 Elf_Data *strings, bool dwarf_64,
				 struct addr_record *die_refs,
				 struct coverage *strings_coverage);
static bool check_aranges_structural (struct read_ctx *ctx,
				      struct cu *cu_chain);


static void
process_file (int fd __attribute__((unused)),
	      Dwarf *dwarf, const char *fname,
	      size_t size __attribute__((unused)),
	      bool only_one)
{
  if (!only_one)
    printf ("\n%s:\n", fname);

  struct read_ctx ctx;

  Elf_Data *abbrev_data = dwarf->sectiondata[IDX_debug_abbrev];
  Elf_Data *info_data = dwarf->sectiondata[IDX_debug_info];
  Elf_Data *aranges_data = dwarf->sectiondata[IDX_debug_aranges];

  /* If we got Dwarf pointer, debug_abbrev and debug_info are present
     inside the file.  But let's be paranoid.  */
  struct abbrev_table *abbrev_chain = NULL;
  if (likely (abbrev_data != NULL))
    {
      read_ctx_init (&ctx, dwarf, abbrev_data);
      abbrev_chain = abbrev_table_load (&ctx);
    }
  else if (!tolerate_nodebug)
    ERROR (".debug_abbrev data not found.");

  struct cu *cu_chain = NULL;

  if (abbrev_chain != NULL)
    {
      Elf_Data *str_data = dwarf->sectiondata[IDX_debug_str];
      /* Same as above...  */
      if (info_data != NULL)
	{
	  read_ctx_init (&ctx, dwarf, info_data);
	  cu_chain = check_debug_info_structural (&ctx, abbrev_chain, str_data);
	}
      else if (!tolerate_nodebug)
	ERROR (".debug_info or .debug_str data not found.");
    }

  if (aranges_data != NULL)
    {
      read_ctx_init (&ctx, dwarf, aranges_data);
      check_aranges_structural (&ctx, cu_chain);
    }
  else
    /* Even though this is an error (XXX ?), it doesn't prevent us
       from doing high-level or low-level checks of other
       sections.  */
    ERROR (".debug_aranges data not found.");

  cu_free (cu_chain);
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
	MESSAGE (mc_leb128 | mc_acc_bloat | mc_impact_3,		\
		 FMT ": unnecessarily long encoding of %s.\n",		\
		 ##ARGS, WHAT);						\
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
	MESSAGE (mc_leb128 | mc_acc_bloat | mc_impact_3,		\
		 FMT ": unnecessarily long encoding of %s.\n",		\
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
read_ctx_eof (struct read_ctx *ctx)
{
  return !read_ctx_need_data (ctx, 1);
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
  uint64_t section_off = 0;

  while (!read_ctx_eof (ctx))
    {
      uint64_t abbr_off, prev_abbr_off = (uint64_t)-1;
      uint64_t abbr_code, prev_abbr_code = (uint64_t)-1;
      uint64_t zero_seq_off = (uint64_t)-1;

      while (!read_ctx_eof (ctx))
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
	MESSAGE_PADDING_0 (mc_abbrevs, PRI_ABBR,
			   zero_seq_off, prev_abbr_off - 1, section_off);

      if (read_ctx_eof (ctx))
	break;

      if (section == NULL)
	{
	  section = xcalloc (1, sizeof (*section));
	  section->offset = abbr_off;
	  section->next = section_chain;
	  section_chain = section;
	  section_off = abbr_off;
	}
      REALLOC (section, abbr);

      struct abbrev *cur = section->abbr + section->size++;
      memset (cur, 0, sizeof (*cur));

      cur->code = abbr_code;

      /* Abbreviation tag.  */
      uint64_t abbr_tag;
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
		    MESSAGE (mc_die_siblings | mc_acc_bloat | mc_impact_1,
			     PRI_ABBR_ATTR
			     ": Excessive DW_AT_sibling attribute at childless abbrev.\n",
			     abbr_off, attr_off);
		}

	      switch (check_sibling_form (attrib_form))
		{
		case -1:
		  MESSAGE (mc_die_siblings | mc_impact_2,
			   PRI_ABBR_ATTR
			   ": DW_AT_sibling attribute with form DW_FORM_ref_addr.\n",
			   abbr_off, attr_off);
		  break;

		case -2:
		  ERROR (PRI_ABBR_ATTR
			 ": DW_AT_sibling attribute with non-reference form %s.\n",
			 abbr_off, attr_off, dwarf_form_string (attrib_form));
		};
	    }

	  acur->name = attrib_name;
	  acur->form = attrib_form;
	  acur->offset = attr_off;
	}
      while (!null_attrib);
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
		      struct addr_record *die_refs)
{
  bool retval = true;
  for (size_t i = 0; i < die_refs->size; ++i)
    {
      uint64_t ref_addr = die_refs->addrs[i];
      if (!addr_record_has_addr (&cu->die_addrs, ref_addr))
	{
	  /* XXX Write which DIE has the reference.  */
	  ERROR (PRI_D_INFO PRI_CU
		 ": unresolved DIE reference to " PRI_DIE ".\n",
		 cu->offset, ref_addr);
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
	uint64_t ref_addr = it->die_refs.addrs[i];
	struct cu *ref_cu = NULL;
	for (struct cu *jt = cu_chain; jt != NULL; jt = jt->next)
	  if (addr_record_has_addr (&jt->die_addrs, ref_addr))
	    {
	      ref_cu = jt;
	      break;
	    }

	/* XXX Write which DIE has the reference.  */
	if (ref_cu == NULL)
	  {
	    ERROR (PRI_D_INFO PRI_CU
		   ": unresolved (global) DIE reference to " PRI_DIE ".\n",
		   it->offset, ref_addr);
	    retval = false;
	  }
	else if (ref_cu == it)
	  /* XXX Turn to MESSAGE.  */
	  WARNING (PRI_D_INFO PRI_CU
		   ": local reference to " PRI_DIE " formed as global.\n",
		   it->offset, ref_addr);
      }

  return retval;
}

#define READ_SIZE_EXTRA(CTX, SIZE32, SIZEP, DWARF_64P, FMT, ARGS...)	\
  ({									\
    __label__ out;							\
    struct read_ctx *_ctx = (CTX);					\
    uint32_t _size32 = (SIZE32);					\
    uint64_t *_sizep = (SIZEP);						\
    bool *_dwarf_64p = (DWARF_64P);					\
    bool _retval = true;						\
									\
    if (_size32 == DWARF3_LENGTH_64_BIT)				\
      {									\
	if (!read_ctx_read_8ubyte (_ctx, _sizep))			\
	  {								\
	    ERROR (FMT ": can't read 64bit CU length.\n", ##ARGS);	\
	    _retval = false;						\
	    goto out;							\
	  }								\
									\
	*_dwarf_64p = true;						\
      }									\
    else if (_size32 >= DWARF3_LENGTH_MIN_ESCAPE_CODE)			\
      {									\
	ERROR (FMT ": unrecognized CU length escape value: %"		\
	       PRIx32 ".\n", ##ARGS, size32);				\
	_retval = false;						\
	goto out;							\
      }									\
    else								\
      *_sizep = _size32;						\
									\
  out:									\
    _retval;								\
  })

#define CHECK_ZERO_PADDING(CTX, CATEGORY, FMT, ARGS...)			\
  ({									\
    __label__ out;							\
    struct read_ctx *_ctx = (CTX);					\
    enum message_category _mc = (CATEGORY);				\
    bool _retval = true;						\
									\
    assert (_ctx->ptr != _ctx->end);					\
    const unsigned char *_save_ptr = _ctx->ptr;				\
    while (!read_ctx_eof (_ctx))					\
      if (*_ctx->ptr++ != 0)						\
	{								\
	  _ctx->ptr = _save_ptr;					\
	  _retval = false;						\
	  goto out;							\
	}								\
									\
    MESSAGE_PADDING_0 (_mc, FMT,					\
		       (uint64_t)(_save_ptr - _ctx->begin),		\
		       (uint64_t)(_ctx->end - _ctx->begin), ##ARGS);	\
									\
  out:									\
    _retval;								\
  })

static struct cu *
check_debug_info_structural (struct read_ctx *ctx,
			     struct abbrev_table *abbrev_chain,
			     Elf_Data *strings)
{
  struct addr_record die_refs;
  memset (&die_refs, 0, sizeof (die_refs));

  struct cu *cu_chain = NULL;

  bool success = true;

  struct coverage strings_coverage_mem;
  struct coverage *strings_coverage = NULL;
  if (strings != NULL && check_category (mc_strings))
    {
      coverage_init (&strings_coverage_mem, strings->d_size);
      strings_coverage = &strings_coverage_mem;
    }

  while (!read_ctx_eof (ctx))
    {
      const unsigned char *cu_begin = ctx->ptr;
      uint64_t cu_off = read_ctx_get_offset (ctx);

      struct cu *cur = xcalloc (1, sizeof (*cur));
      cur->offset = cu_off;
      cur->next = cu_chain;
      cu_chain = cur;

      uint32_t size32;
      uint64_t size;
      bool dwarf_64 = false;

      /* Reading CU header is a bit tricky, because we don't know if
	 we have run into (superfluous but allowed) zero padding.  */
      if (!read_ctx_need_data (ctx, 4)
	  && CHECK_ZERO_PADDING (ctx, mc_die_other, PRI_D_INFO PRI_CU, cu_off))
	break;

      /* CU length.  */
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  ERROR (PRI_D_INFO PRI_CU ": can't read CU length.\n", cu_off);
	  success = false;
	  break;
	}
      if (size32 == 0 && CHECK_ZERO_PADDING (ctx, mc_die_other,
					     PRI_D_INFO PRI_CU, cu_off))
	break;

      if (!READ_SIZE_EXTRA (ctx, size32, &size, &dwarf_64,
			    PRI_D_INFO PRI_CU, cu_off))
	{
	  success = false;
	  break;
	}

      if (!read_ctx_need_data (ctx, size))
	{
	  ERROR (PRI_D_INFO PRI_CU ": section doesn't have enough data"
		 " to read CU of size %" PRIx64 ".\n", cu_off, size);
	  ctx->ptr = ctx->end;
	  success = false;
	  break;
	}

      /* version + debug_abbrev_offset + address_size */
      uint64_t cu_header_size = 2 + (dwarf_64 ? 8 : 4) + 1;
      if (size < cu_header_size)
	{
	  ERROR (PRI_D_INFO PRI_CU ": claimed length of %" PRIx64
		 " doesn't even cover CU header.\n", cu_off, size);
	  success = false;
	  break;
	}
      else
	{
	  /* Make CU context begin just before the CU length, so that DIE
	     offsets are computed correctly.  */
	  struct read_ctx cu_ctx;
	  const unsigned char *cu_end = ctx->ptr + size;
	  read_ctx_init_sub (&cu_ctx, ctx->dbg, ctx->data, cu_begin, cu_end);
	  cu_ctx.ptr = ctx->ptr;

	  if (!check_cu_structural (&cu_ctx, cur, abbrev_chain, strings,
				    dwarf_64, &die_refs, strings_coverage))
	    {
	      success = false;
	      break;
	    }
	  if (cu_ctx.ptr != cu_ctx.end
	      && !CHECK_ZERO_PADDING (&cu_ctx, mc_die_other,
				      PRI_D_INFO PRI_CU, cu_off))
	    MESSAGE_PADDING_N0 (mc_die_other, PRI_D_INFO PRI_CU,
				read_ctx_get_offset (ctx),
				size, cu_off);
	}

      ctx->ptr += size;
    }

  // Only check this if above we have been successful.
  if (success && ctx->ptr != ctx->end)
    MESSAGE (mc_die_other | mc_impact_4,
	     ".debug_info: CU lengths don't exactly match Elf_Data contents.");

  bool references_sound = check_global_die_references (cu_chain);
  addr_record_free (&die_refs);

  if (strings_coverage != NULL)
    {
      void hole (uint64_t begin, uint64_t end)
      {
	bool all_zeroes = true;
	for (uint64_t i = begin; i <= end; ++i)
	  if (((char*)strings->d_buf)[i] != 0)
	    {
	      all_zeroes = false;
	      break;
	    }

	if (all_zeroes)
	  MESSAGE_PADDING_0 (mc_strings, ".debug_str", begin, end);
	else
	  /* XXX: This is actually lying in case that the unreferenced
	     portion is composed of sequences of zeroes and non-zeroes.  */
	  MESSAGE_PADDING_N0 (mc_strings, ".debug_str", begin, end);
      }

      if (success)
	coverage_find_holes (strings_coverage, hole);
      coverage_free (strings_coverage);
    }

  if (!success || !references_sound)
    {
      cu_free (cu_chain);
      cu_chain = NULL;
    }

  return cu_chain;
}


/*
  Returns:
    -1 in case of error
    +0 in case of no error, but the chain only consisted of a
       terminating zero die.
    +1 in case some dies were actually loaded
 */
static int
read_die_chain (struct read_ctx *ctx,
		struct cu *cu,
		struct abbrev_table *abbrevs, Elf_Data *strings,
		bool dwarf_64, bool addr_64,
		struct addr_record *die_refs,
		struct addr_record *die_loc_refs,
		struct coverage *strings_coverage)
{
  bool got_die = false;
  const unsigned char *begin = ctx->ptr;
  uint64_t sibling_addr = 0;
  uint64_t die_off, prev_die_off = 0;
  struct abbrev *abbrev, *prev_abbrev = NULL;

  while (!read_ctx_eof (ctx))
    {
      uint64_t abbr_code;

      prev_die_off = die_off;
      die_off = read_ctx_get_offset (ctx);
      if (!CHECKED_READ_ULEB128 (ctx, &abbr_code,
				 PRI_D_INFO PRI_CU_DIE, "abbrev code",
				 cu->offset, die_off))
	return -1;

      /* Check sibling value advertised last time through the loop.  */
      if (sibling_addr != 0)
	{
	  if (abbr_code == 0)
	    ERROR (PRI_D_INFO PRI_CU_DIE
		   ": is the last sibling in chain, but has a DW_AT_sibling attribute.\n",
		   cu->offset, prev_die_off);
	  else if (sibling_addr != die_off)
	    ERROR (PRI_D_INFO PRI_CU_DIE
		   ": This DIE should have had its sibling at 0x%"
		   PRIx64 ", but it's at 0x%" PRIx64 " instead.\n",
		   cu->offset, prev_die_off, sibling_addr, die_off);
	  sibling_addr = 0;
	}
      else if (prev_abbrev != NULL && prev_abbrev->has_children)
	/* Even if it has children, the DIE can't have a sibling
	   attribute if it's the last DIE in chain.  That's the reason
	   we can't simply check this when loading abbrevs.  */
	MESSAGE (mc_die_siblings | mc_acc_suboptimal | mc_impact_4,
		 PRI_D_INFO PRI_CU_DIE
		 ": This DIE had children, but no DW_AT_sibling attribute.\n",
		 cu->offset, prev_die_off);

      /* The section ended.  */
      if (read_ctx_eof (ctx) || abbr_code == 0)
	{
	  if (abbr_code != 0)
	    ERROR (PRI_D_INFO PRI_CU
		   ": DIE chain at %p not terminated with DIE with zero abbrev code.\n",
		   cu->offset, begin);
	  break;
	}

      prev_die_off = die_off;
      got_die = true;

      /* Find the abbrev matching the code.  */
      abbrev = abbrev_table_find_abbrev (abbrevs, abbr_code);
      if (abbrev == NULL)
	{
	  ERROR (PRI_D_INFO PRI_CU_DIE ": abbrev section at 0x%" PRIx64
		 " doesn't contain code %" PRIu64 ".\n",
		 cu->offset, die_off, abbrevs->offset, abbr_code);
	  return -1;
	}
      abbrev->used = true;

      addr_record_add (&cu->die_addrs, cu->offset + die_off);

      /* Attribute values.  */
      for (struct abbrev_attrib *it = abbrev->attribs;
	   it->name != 0; ++it)
	{

	  void record_ref (uint64_t addr, bool local)
	  {
	    struct addr_record *record = &cu->die_refs;
	    if (local)
	      {
		assert (ctx->end > ctx->begin);
		if (addr > (uint64_t)(ctx->end - ctx->begin))
		  {
		    ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			   ": Invalid reference outside the CU: 0x%" PRIx64 ".\n",
			   cu->offset, die_off, abbrev->code, it->offset, addr);
		    return;
		  }

		/* Address holds a CU-local reference, so add CU
		   offset to turn it into section offset.  */
		addr += cu->offset;
		record = die_loc_refs;
	      }

	    if (record != NULL)
	      addr_record_add (record, addr);
	  }

	  uint8_t form = it->form;
	  if (form == DW_FORM_indirect)
	    {
	      uint64_t value;
	      if (!CHECKED_READ_ULEB128 (ctx, &value,
					 PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					 "indirect attribute form",
					 cu->offset, die_off, abbrev->code,
					 it->offset))
		return -1;

	      if (!attrib_form_valid (value))
		{
		  ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			 ": invalid indirect form 0x%" PRIx64 ".\n",
			 cu->offset, die_off, abbrev->code, it->offset, value);
		  return -1;
		}
	      form = value;

	      if (it->name == DW_AT_sibling)
		switch (check_sibling_form (form))
		  {
		  case -1:
		    MESSAGE (mc_die_siblings | mc_impact_2,
			     PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			     ": DW_AT_sibling attribute with (indirect) form DW_FORM_ref_addr.\n",
			     cu->offset, die_off, abbrev->code, it->offset);
		    break;

		  case -2:
		    ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			   ": DW_AT_sibling attribute with non-reference (indirect) form %s.\n",
			   cu->offset, die_off, abbrev->code, it->offset,
			   dwarf_form_string (value));
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
		    ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			   ": can't read attribute value.\n",
			   cu->offset, die_off, abbrev->code, it->offset);
		    return -1;
		  }

		if (strings == NULL)
		  ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			 ": strp attribute, but no .debug_str section.\n",
			 cu->offset, die_off, abbrev->code, it->offset);
		else if (addr >= strings->d_size)
		  ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			 ": Invalid offset outside .debug_str: 0x%" PRIx64 ".",
			 cu->offset, die_off, abbrev->code, it->offset, addr);
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
		if (!CHECKED_READ_ULEB128 (ctx, &value,
					   PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					   "attribute value",
					   cu->offset, die_off, abbrev->code,
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
		if (!CHECKED_READ_SLEB128 (ctx, &value,
					   PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					   "attribute value",
					   cu->offset, die_off, abbrev->code,
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
		    if (!CHECKED_READ_ULEB128 (ctx, &length,
					       PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					       "attribute value",
					       cu->offset, die_off, abbrev->code,
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
	      ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
		     ": Indirect form is again indirect.\n",
		     cu->offset, die_off, abbrev->code, it->offset);
	      return -1;

	    default:
	      ERROR (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
		     ": Internal error: unhandled form 0x%x\n",
		     cu->offset, die_off, abbrev->code, it->offset, it->form);
	    }
	}

      if (abbrev->has_children)
	{
	  int st = read_die_chain (ctx, cu, abbrevs, strings,
				   dwarf_64, addr_64,
				   die_refs, die_loc_refs,
				   strings_coverage);
	  if (st == -1)
	    return -1;
	  else if (st == 0)
	    MESSAGE (mc_die_children | mc_acc_suboptimal | mc_impact_3,
		     PRI_D_INFO PRI_CU_DIE
		     ": Abbrev has_children, but the chain was empty.\n",
		     cu->offset, die_off);
	}
    }

  if (sibling_addr != 0)
    ERROR (PRI_D_INFO PRI_CU_DIE
	   ": This DIE should have had its sibling at 0x%"
	   PRIx64 ", but the DIE chain ended.\n",
	   cu->offset, prev_die_off, sibling_addr);

  return got_die ? 1 : 0;
}

#define READ_VERSION(CTX, DWARF_64, VERSIONP, FMT, ARGS...)		\
  ({									\
    __label__ out;							\
    struct read_ctx *_ctx = (CTX);					\
    bool _dwarf_64 = (DWARF_64);					\
    uint16_t *_versionp = (VERSIONP);					\
    bool _retval = true;						\
    									\
    if (!read_ctx_read_2ubyte (_ctx, _versionp))			\
      {									\
	ERROR (FMT ": can't read version.\n", ##ARGS);			\
	_retval = false;						\
	goto out;							\
      }									\
									\
    if (*_versionp < 2 || *_versionp > 3)				\
      {									\
	ERROR (FMT ": %s version %d.\n",				\
	       ##ARGS, (*_versionp < 2 ? "invalid" : "unsupported"),	\
	       *_versionp);						\
	_retval = false;						\
	goto out;							\
      }									\
									\
    if (*_versionp == 2 && _dwarf_64)					\
      /* Keep going.  It's a standard violation, but we may still be	\
	 able to read the unit under considertaion and do high-level	\
	 checks.  */							\
      ERROR (FMT ": invalid 64-bit unit in DWARF 2 format.\n", ##ARGS);	\
									\
  out:									\
    _retval;								\
  })

static bool
check_cu_structural (struct read_ctx *ctx,
		     struct cu *const cu,
		     struct abbrev_table *abbrev_chain,
		     Elf_Data *strings, bool dwarf_64,
		     struct addr_record *die_refs,
		     struct coverage *strings_coverage)
{
  uint16_t version;
  uint64_t abbrev_offset;
  uint8_t address_size;

  /* Version.  */
  if (!READ_VERSION (ctx, dwarf_64, &version, PRI_D_INFO PRI_CU, cu->offset))
    return false;

  /* Abbrev offset.  */
  if (!read_ctx_read_offset (ctx, dwarf_64, &abbrev_offset))
    {
      ERROR (PRI_D_INFO PRI_CU ": can't read abbrev offset.\n", cu->offset);
      return false;
    }

  /* Address size.  */
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      ERROR (PRI_D_INFO PRI_CU ": can't read address size.\n", cu->offset);
      return false;
    }
  if (address_size != 4 && address_size != 8)
    {
      ERROR (PRI_D_INFO PRI_CU
	     ": Invalid address size: %d (only 4 or 8 allowed).\n",
	     cu->offset, address_size);
      return false;
    }

  struct abbrev_table *abbrevs = abbrev_chain;
  for (; abbrevs != NULL; abbrevs = abbrevs->next)
    if (abbrevs->offset == abbrev_offset)
      break;

  if (abbrevs == NULL)
    {
      ERROR (PRI_D_INFO PRI_CU
	     ": Couldn't find abbrev section with offset 0x%" PRIx64 ".\n",
	     cu->offset, abbrev_offset);
      return false;
    }

  struct addr_record die_loc_refs;
  memset (&die_loc_refs, 0, sizeof (die_loc_refs));

  bool retval = true;
  if (read_die_chain (ctx, cu, abbrevs, strings,
		      dwarf_64, address_size == 8,
		      die_refs, &die_loc_refs,
		      strings_coverage) >= 0)
    {
      for (size_t i = 0; i < abbrevs->size; ++i)
	if (!abbrevs->abbr[i].used)
	  MESSAGE (mc_impact_3 | mc_acc_bloat | mc_abbrevs,
		   PRI_D_INFO PRI_CU ": Abbreviation with code %"
		   PRIu64 " is never used.\n",
		   cu->offset, abbrevs->abbr[i].code);

      if (!check_die_references (cu, &die_loc_refs))
	retval = false;
    }
  else
    retval = false;

  addr_record_free (&die_loc_refs);
  return retval;
}


static bool
check_aranges_structural (struct read_ctx *ctx,
			  struct cu *cu_chain)
{
  while (!read_ctx_eof (ctx))
    {
      uint64_t atab_off = read_ctx_get_offset (ctx);

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      bool dwarf_64;
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  ERROR (PRI_D_ARANGES PRI_ARANGETAB
		 ": can't read unit length.\n", atab_off);
	  return false;
	}
      /*
      if (size32 == 0 && check_zero_padding (ctx))
	break;
      */

      if (!READ_SIZE_EXTRA (ctx, size32, &size, &dwarf_64,
			    PRI_D_ARANGES PRI_ARANGETAB, atab_off))
	return false;


      /* Version.  */
      uint16_t version;
      if (!READ_VERSION (ctx, dwarf_64, &version,
			 PRI_D_ARANGES PRI_ARANGETAB, atab_off))
	return false;

      /* CU offset.  */
      uint64_t cu_off;
      if (!read_ctx_read_offset (ctx, dwarf_64, &cu_off))
	{
	  ERROR (PRI_D_ARANGES PRI_ARANGETAB
		 ": can't read debug info offset.\n", atab_off);
	  return false;
	}
      if (cu_find_cu (cu_chain, cu_off) == NULL)
	ERROR (PRI_D_ARANGES PRI_ARANGETAB
	       ": invalid reference to " PRI_CU ".\n", atab_off, cu_off);

      /* Address size.  */
      uint8_t address_size;
      if (!read_ctx_read_ubyte (ctx, &address_size))
	{
	  ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU
		 ": can't read unit address size.\n", atab_off, cu_off);
	  return false;
	}
      if (address_size != 2
	  && address_size != 4
	  && address_size != 8)
	{
	  /* XXX Does anyone need e.g. 6 byte addresses?  */
	  ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU
		 ": invalid address size: %d.\n",
		 atab_off, cu_off, address_size);
	  return false;
	}

      /* Segment size.  */
      uint8_t segment_size;
      if (!read_ctx_read_ubyte (ctx, &segment_size))
	{
	  ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU
		 ": can't read unit segment size.\n", atab_off, cu_off);
	  return false;
	}
      if (segment_size != 0)
	{
	  WARNING (PRI_D_ARANGES PRI_ARANGETAB_CU
		   ": dwarflint can't handle segment_size != 0.\n",
		   atab_off, cu_off);
	  return false;
	}


      /* 7.20: The first tuple following the header in each set begins
	 at an offset that is a multiple of the size of a single tuple
	 (that is, twice the size of an address). The header is
	 padded, if necessary, to the appropriate boundary.  */
      const uint8_t tuple_size = 2 * address_size;
      uint64_t off = read_ctx_get_offset (ctx);
      if ((off % tuple_size) != 0)
	{
	  uint64_t noff = ((off / tuple_size) + 1) * tuple_size;
	  for (uint64_t i = off; i < noff; ++i)
	    {
	      uint8_t c;
	      if (!read_ctx_read_ubyte (ctx, &c))
		{
		  ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU
			 ": section ends after the header, but before the first entry.\n",
			 atab_off, cu_off);
		  return false;
		}
	      if (c != 0)
		MESSAGE (mc_impact_2 | mc_aranges,
			 PRI_D_ARANGES PRI_ARANGETAB_CU
			 ": non-zero byte at 0x%" PRIx64
			 " in padding before the first entry.\n",
			 atab_off, cu_off, read_ctx_get_offset (ctx));
	    }
	}
      assert ((read_ctx_get_offset (ctx) % tuple_size) == 0);

      while (!read_ctx_eof (ctx))
	{
	  uint64_t tuple_off = read_ctx_get_offset (ctx);
	  uint64_t address, length;
	  if (!read_ctx_read_var (ctx, address_size, &address))
	    {
	      ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU_RECORD
		     ": can't read address field.\n",
		     atab_off, cu_off, tuple_off);
	      return false;
	    }
	  if (!read_ctx_read_var (ctx, address_size, &length))
	    {
	      ERROR (PRI_D_ARANGES PRI_ARANGETAB_CU_RECORD
		     ": can't read length field.\n",
		     atab_off, cu_off, tuple_off);
	      return false;
	    }

	  if (address == 0 && length == 0)
	    break;

	  /* Address and length can be validated on high level.  */
	}
    }

  return true;
}
