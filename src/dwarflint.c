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
#include <stdarg.h>
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
  mc_impact_1  = 0x1, // no impact on the consumer
  mc_impact_2  = 0x2, // still no impact, but suspicious or worth mentioning
  mc_impact_3  = 0x4, // some impact
  mc_impact_4  = 0x8, // high impact
  mc_impact_all= 0xf, // all severity levels
  mc_impact_2p = 0xe, // 2+
  mc_impact_3p = 0xc, // 3+

  /* Accuracy:  */
  mc_acc_bloat     = 0x10, // unnecessary constructs (e.g. unreferenced strings)
  mc_acc_suboptimal= 0x20, // suboptimal construct (e.g. lack of siblings)
  mc_acc_all       = 0x30, // all accuracy options

  /* Various: */
  mc_error     = 0x40,  // make the message into an error

  /* Area: */
  mc_leb128    = 0x100, // ULEB/SLEB storage
  mc_abbrevs   = 0x200, // abbreviations and abbreviation tables
  mc_die_rel_sib  = 0x1000, // DIE sibling relationship
  mc_die_rel_child= 0x2000, // DIE parent/child relationship
  mc_die_rel_ref  = 0x4000, // relationship by reference
  mc_die_rel_all  = 0x7000, // any DIE/DIE relationship
  mc_die_other    = 0x8000, // other messages related to DIEs and .debug_info tables
  mc_die_all      = 0xf000, // includes all DIE categories
  mc_strings   = 0x10000, // string table
  mc_aranges   = 0x20000, // address ranges table
  mc_elf       = 0x40000, // ELF structure, e.g. missing optional sections
  mc_pubtables = 0x80000, // table of public names/types
  mc_pubtypes  = 0x100000, // .debug_pubtypes presence
  mc_loc       = 0x200000, // messages related to .debug_loc
  mc_other     = 0x400000, // messages unrelated to any of the above
  mc_all       = 0xffffff00, // all areas
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

static struct message_criteria warning_criteria
  = {mc_all & ~(mc_strings | mc_loc),
     mc_pubtypes};
static struct message_criteria error_criteria
  = {mc_impact_4 | mc_error,
     mc_none};
static unsigned error_count = 0;

static bool
check_category (enum message_category cat)
{
  return accept_message (&warning_criteria, cat);
}

static char fmterr[] = "(fmt error)";

static void __attribute__ ((format (printf, 2, 4)))
vverror (const char *fmt1, const char *fmt2, va_list ap1, ...)
{
  fputs ("error: ", stdout);
  vprintf (fmt1, ap1);

  va_list ap2;
  va_start (ap2, ap1);
  vprintf (fmt2, ap2);
  va_end (ap2);

  ++error_count;
}

static void
verror (const char *format, va_list ap)
{
  vverror (format, "%s", ap, "");
}

static void __attribute__ ((format (printf, 1, 2)))
wr_error (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  verror (format, ap);
  va_end (ap);
}

static void
vwarning (const char *format, va_list ap)
{
  fputs ("warning: ", stdout);
  vprintf (format, ap);
  ++error_count;
}

static void
vmessage (enum message_category category, const char *format, va_list ap)
{
  if (accept_message (&warning_criteria, category))
    {
      if (accept_message (&error_criteria, category))
	verror (format, ap);
      else
	vwarning (format, ap);
    }
}

static void __attribute__ ((format (printf, 1, 2)))
wr_warning (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vwarning (format, ap);
  va_end (ap);
}

static void __attribute__ ((format (printf, 2, 3)))
message (enum message_category category, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vmessage (category, format, ap);
  va_end (ap);
}

static void
vfmessage (enum message_category category,
	   const char *fmt1, const char *fmt2, va_list ap1, ...)
{
  if (!accept_message (&warning_criteria, category))
    return;

  char *buf1, *buf2;
  if (vasprintf (&buf1, fmt1, ap1) < 0)
    buf1 = NULL;

  va_list ap2;
  va_start (ap2, ap1);
  if (vasprintf (&buf2, fmt2, ap2) < 0)
    buf2 = NULL;
  message (category, "%s%s", buf1 ?: fmterr, buf2 ?: fmterr);
  va_end (ap2);

  free (buf1);
  free (buf2);
}

static void
format_padding_message (enum message_category category,
			uint64_t start, uint64_t end,
			char *kind, const char *format, va_list ap)
{
  vfmessage (category, format,
	     ": 0x%" PRIx64 "..0x%" PRIx64 ": %s.\n",
	     ap, start, end, kind);
}

static void
format_leb128_message (int st, const char *format, const char *what, va_list ap)
{
  enum message_category category = mc_leb128 | mc_acc_bloat | mc_impact_3;
  if (st == 0 || (st > 0 && !accept_message (&warning_criteria, category)))
    return;

  if (st < 0)
    vverror (format, ": can't read %s.\n", ap, what ?: "");
  else if (st > 0)
    vfmessage (category, format,
	       ": unnecessarily long encoding of %s.\n", ap, what ?: "");
}

static void
vmessage_padding_0 (enum message_category category,
		    uint64_t start, uint64_t end,
		    const char *format, va_list ap)
{
  format_padding_message (category | mc_acc_bloat | mc_impact_1,
			  start, end,
			  "unnecessary padding with zero bytes",
			  format, ap);
}

static void
message_padding_0 (enum message_category category,
		   uint64_t start, uint64_t end,
		   const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  vmessage_padding_0 (category, start, end, format, ap);
  va_end (ap);
}

static void
message_padding_n0 (enum message_category category,
		    uint64_t start, uint64_t end,
		    const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  format_padding_message (category | mc_acc_bloat | mc_impact_2,
			  start, end,
			  "unreferenced non-zero bytes",
			  format, ap);
  va_end (ap);
}

/* True if no message is to be printed if the run is succesful.  */
static bool be_quiet;
static bool be_strict = false; /* --strict */
static bool be_gnu = false; /* --gnu */

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

  if (tolerate_nodebug)
    warning_criteria.reject |= mc_elf;
  if (be_gnu)
    warning_criteria.reject |= mc_acc_bloat | mc_pubtypes;
  if (be_strict)
    {
      warning_criteria.accept |= mc_strings | mc_loc;
      if (!be_gnu)
	warning_criteria.reject &= ~mc_pubtypes;
    }

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
	wr_error (gettext ("cannot generate Elf descriptor: %s\n"),
		  elf_errmsg (-1));
      else
	{
	  unsigned int prev_error_count = error_count;
	  Dwarf *dwarf = dwarf_begin_elf (elf, DWARF_C_READ, NULL);
	  if (dwarf == NULL)
	    {
	      if (!tolerate_nodebug)
		wr_error (gettext ("cannot generate Dwarf descriptor: %s\n"),
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
		wr_error (gettext ("error while closing Dwarf descriptor: %s\n"),
			  dwarf_errmsg (-1));
	    }

	  if (elf_end (elf) != 0)
	    wr_error (gettext ("error while closing Elf descriptor: %s\n"),
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

    case ARGP_gnu:
      be_gnu = true;
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
#define PRI_D_PUBNAMES ".debug_pubnames: "
#define PRI_D_PUBTYPES ".debug_pubtypes: "
#define PRI_D_STR ".debug_str: "
#define PRI_D_LOC ".debug_loc: "
#define PRI_LOCEXPR "location expression: "

#define PRI_CU "CU 0x%" PRIx64
#define PRI_DIE "DIE 0x%" PRIx64
#define PRI_ATTR "attribute 0x%" PRIx64
#define PRI_ABBR "abbrev 0x%" PRIx64
#define PRI_ARANGETAB "arange table 0x%" PRIx64
#define PRI_RECORD "record 0x%" PRIx64
#define PRI_PUBSET "pubset 0x%" PRIx64 // for pubnames and pubtypes

#define PRI_CU_DIE PRI_CU ", " PRI_DIE
#define PRI_CU_DIE_ABBR_ATTR PRI_CU_DIE ", " PRI_ABBR ", " PRI_ATTR
#define PRI_ABBR_ATTR PRI_ABBR ", " PRI_ATTR
#define PRI_ARANGETAB_CU PRI_ARANGETAB " (for " PRI_CU ")"
#define PRI_ARANGETAB_CU_RECORD PRI_ARANGETAB_CU ", " PRI_RECORD
#define PRI_PUBSET_CU PRI_PUBSET " (for " PRI_CU ")"
#define PRI_PUBSET_CU_RECORD PRI_PUBSET_CU ", " PRI_RECORD

#define PRI_NOT_ENOUGH ": not enough data for %s.\n"
#define PRI_CAUSE ": caused by this reference.\n"

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
static bool read_ctx_init_sub (struct read_ctx *ctx,
			       struct read_ctx *parent,
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


/* Functions and data structures for reference handling.  Just like
   the above, we use this to check validity of DIE references.  Unlike
   the above, this is not stored as sorted set, but simply as an array
   of records, because duplicates are unlikely.  */

struct ref
{
  uint64_t addr; // Referree address
  uint64_t who;  // Referrer address
};

struct ref_record
{
  size_t size;
  size_t alloc;
  struct ref *refs;
};

static void ref_record_add (struct ref_record *rr, uint64_t addr, uint64_t who);
static void ref_record_free (struct ref_record *rr);


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

struct hole_info
{
  const char *section;
  enum message_category category;
  void *d_buf;
};

static void coverage_init (struct coverage *ar, uint64_t size);
static void coverage_add (struct coverage *ar, uint64_t begin, uint64_t end);
static bool coverage_is_covered (struct coverage *ar, uint64_t address);
static void coverage_find_holes (struct coverage *ar,
				 void (*cb)(uint64_t begin, uint64_t end, void *data),
				 void *data);
static void found_hole (uint64_t begin, uint64_t end, void *data);
static void coverage_free (struct coverage *ar);


/* Functions and data structures for CU handling.  */

struct cu
{
  uint64_t offset;
  uint64_t length;
  struct addr_record die_addrs; // Addresses where DIEs begin in this CU.
  struct ref_record die_refs;   // DIE references into other CUs from this CU.
  struct cu *next;
};

static void cu_free (struct cu *cu_chain);
static struct cu *cu_find_cu (struct cu *cu_chain, uint64_t offset);


/* Functions for checking of structural integrity.  */

static struct cu *check_debug_info_structural (struct read_ctx *ctx,
					       struct abbrev_table *abbrev_chain,
					       Elf_Data *strings,
					       Elf_Data *loc);
static bool check_cu_structural (struct read_ctx *ctx,
				 struct cu *const cu,
				 struct abbrev_table *abbrev_chain,
				 Elf_Data *strings,
				 Elf_Data *loc,
				 bool dwarf_64,
				 struct ref_record *die_refs,
				 struct addr_record *loc_addrs,
				 struct coverage *strings_coverage,
				 struct coverage *loc_coverage);
static bool check_aranges_structural (struct read_ctx *ctx,
				      struct cu *cu_chain);
static bool check_pub_structural (struct read_ctx *ctx,
				  struct cu *cu_chain,
				  const char *secname);


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
  Elf_Data *pubnames_data = dwarf->sectiondata[IDX_debug_pubnames];
  Elf_Data *loc_data = dwarf->sectiondata[IDX_debug_loc];

  /* Obtaining pubtypes is a bit complicated, because GNU toolchain
     doesn't emit it, and libdw doesn't account for it.  */
  Elf_Scn *scn = NULL;
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (dwarf->elf, &ehdr_mem);
  if (ehdr == NULL)
    goto invalid_elf;
  Elf_Data *pubtypes_data = NULL;
  while ((scn = elf_nextscn (dwarf->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem, *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	goto invalid_elf;

      const char *scnname = elf_strptr (dwarf->elf, ehdr->e_shstrndx,
					shdr->sh_name);
      if (scnname == NULL)
	{
	invalid_elf:
	  /* A "can't happen".  libdw already managed to parse the Elf
	     file when constructing the Dwarf object.  */
	  wr_error ("broken Elf");
	  break;
	}
      if (strcmp (scnname, ".debug_pubtypes") == 0)
	{
	  pubtypes_data = elf_getdata (scn, NULL);
	  break;
	}
    }

  /* If we got Dwarf pointer, debug_abbrev and debug_info are present
     inside the file.  But let's be paranoid.  */
  struct abbrev_table *abbrev_chain = NULL;
  if (likely (abbrev_data != NULL))
    {
      read_ctx_init (&ctx, dwarf, abbrev_data);
      abbrev_chain = abbrev_table_load (&ctx);
    }
  else if (!tolerate_nodebug)
    /* Hard error, not a message.  We can't debug without this.  */
    wr_error (".debug_abbrev data not found.\n");

  struct cu *cu_chain = NULL;

  if (abbrev_chain != NULL)
    {
      Elf_Data *str_data = dwarf->sectiondata[IDX_debug_str];
      /* Same as above...  */
      if (info_data != NULL)
	{
	  read_ctx_init (&ctx, dwarf, info_data);
	  cu_chain = check_debug_info_structural (&ctx, abbrev_chain,
						  str_data, loc_data);
	}
      else if (!tolerate_nodebug)
	/* Hard error, not a message.  We can't debug without this.  */
	wr_error (".debug_info data not found.\n");
    }

  if (aranges_data != NULL)
    {
      read_ctx_init (&ctx, dwarf, aranges_data);
      check_aranges_structural (&ctx, cu_chain);
    }
  else
    message (mc_impact_4 | mc_acc_suboptimal | mc_elf,
	     ".debug_aranges data not found.\n");

  if (pubnames_data != NULL)
    {
      read_ctx_init (&ctx, dwarf, pubnames_data);
      check_pub_structural (&ctx, cu_chain, PRI_D_PUBNAMES);
    }
  else
    message (mc_impact_4 | mc_acc_suboptimal | mc_elf,
	     PRI_D_PUBNAMES "data not found.\n");

  if (pubtypes_data != NULL)
    {
      read_ctx_init (&ctx, dwarf, pubtypes_data);
      check_pub_structural (&ctx, cu_chain, PRI_D_PUBTYPES);
    }
  else
    message (mc_impact_4 | mc_acc_suboptimal | mc_elf | mc_pubtypes,
	     PRI_D_PUBTYPES "data not found.\n");

  cu_free (cu_chain);
  abbrev_table_free (abbrev_chain);
}

static void
read_ctx_init (struct read_ctx *ctx, Dwarf *dbg, Elf_Data *data)
{
  if (data == NULL)
    abort ();

  ctx->dbg = dbg;
  ctx->data = data;
  ctx->begin = data->d_buf;
  ctx->end = data->d_buf + data->d_size;
  ctx->ptr = data->d_buf;
}

static bool
read_ctx_init_sub (struct read_ctx *ctx, struct read_ctx *parent,
		   const unsigned char *begin, const unsigned char *end)
{
  if (parent == NULL)
    abort ();

  if (begin < parent->begin
      || end > parent->end)
    return false;

  ctx->dbg = parent->dbg;
  ctx->data = parent->data;
  ctx->begin = begin;
  ctx->end = end;
  ctx->ptr = begin;
  return true;
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
  if (ret != NULL)
    *ret = *ctx->ptr;
  ctx->ptr++;
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

  if (ret != NULL)
    *ret = result;
  return zero_tail ? 1 : 0;
}

static bool
vchecked_read_uleb128 (struct read_ctx *ctx, uint64_t *ret,
		       const char *format, const char *what, va_list ap)
{
  int st = read_ctx_read_uleb128 (ctx, ret);
  format_leb128_message (st, format, what, ap);
  return st >= 0;
}

static bool __attribute__ ((format (printf, 3, 5)))
checked_read_uleb128 (struct read_ctx *ctx, uint64_t *ret,
		      const char *format, const char *what, ...)
{
  va_list ap;
  va_start (ap, what);
  bool retval = vchecked_read_uleb128 (ctx, ret, format, what, ap);
  va_end (ap);
  return retval;
}

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

  if (ret != NULL)
    *ret = result;
  return zero_tail ? 1 : 0;
}

static bool
vchecked_read_sleb128 (struct read_ctx *ctx, int64_t *ret,
		       const char *format, const char *what, va_list ap)
{
  int st = read_ctx_read_sleb128 (ctx, ret);
  format_leb128_message (st, format, what, ap);
  return st >= 0;
}

static bool __attribute__ ((format (printf, 3, 5)))
checked_read_sleb128 (struct read_ctx *ctx, int64_t *ret,
		      const char *format, const char *what, ...)
{
  va_list ap;
  va_start (ap, what);
  bool retval = vchecked_read_sleb128 (ctx, ret, format, what, ap);
  va_end (ap);
  return retval;
}

static bool
read_ctx_read_2ubyte (struct read_ctx *ctx, uint16_t *ret)
{
  if (!read_ctx_need_data (ctx, 2))
    return false;
  uint16_t val = read_2ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  if (ret != NULL)
    *ret = val;
  return true;
}

static bool
read_ctx_read_4ubyte (struct read_ctx *ctx, uint32_t *ret)
{
  if (!read_ctx_need_data (ctx, 4))
    return false;
  uint32_t val = read_4ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  if (ret != NULL)
    *ret = val;
  return true;
}

static bool
read_ctx_read_8ubyte (struct read_ctx *ctx, uint64_t *ret)
{
  if (!read_ctx_need_data (ctx, 8))
    return false;
  uint64_t val = read_8ubyte_unaligned_inc (ctx->dbg, ctx->ptr);
  if (ret != NULL)
    *ret = val;
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

  if (ret != NULL)
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

/* Check that given form is in fact valid in concrete CU.  Return 0 if
   it's absolutely invalid, -1 if it's invalid in the given context, 1
   if it's valid loclistptr, 2 if it's valid block.  */
static int
check_CU_location_form (uint64_t form, bool dwarf_64)
{
  switch (form)
    {
      /* loclistptr */
    case DW_FORM_data4:
      if (dwarf_64)
	return -1;
      return 1;

    case DW_FORM_data8:
      if (!dwarf_64)
	return -1;
      return 1;

      /* block */
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
      return 2;

    default:
      return 0;
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
	  if (!checked_read_uleb128 (ctx, &abbr_code,
				     PRI_D_ABBREV PRI_ABBR,
				     "abbrev code", abbr_off))
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
	message_padding_0 (mc_abbrevs, zero_seq_off, prev_abbr_off - 1,
			   PRI_D_ABBREV PRI_ABBR, section_off);

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
      if (!checked_read_uleb128 (ctx, &abbr_tag,
				 PRI_D_ABBREV PRI_ABBR,
				 "abbrev tag", abbr_off))
	goto free_and_out;

      if (abbr_tag > DW_TAG_hi_user)
	{
	  wr_error (PRI_D_ABBREV PRI_ABBR
		    ": invalid abbrev tag 0x%" PRIx64 ".\n",
		    abbr_off, abbr_tag);
	  goto free_and_out;
	}
      cur->tag = (typeof (cur->tag))abbr_tag;

      /* Abbreviation has_children.  */
      uint8_t has_children;
      if (!read_ctx_read_ubyte (ctx, &has_children))
	{
	  wr_error (PRI_D_ABBREV PRI_ABBR
		    ": can't read abbrev has_children.\n", abbr_off);
	  goto free_and_out;
	}

      if (has_children != DW_CHILDREN_no
	  && has_children != DW_CHILDREN_yes)
	{
	  wr_error (PRI_D_ABBREV PRI_ABBR
		    ": invalid has_children value 0x%x.\n",
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
	  if (!checked_read_uleb128 (ctx, &attrib_name,
				     PRI_D_ABBREV PRI_ABBR_ATTR,
				     "attribute name",
				     abbr_off, attr_off))
	    goto free_and_out;

	  if (!checked_read_uleb128 (ctx, &attrib_form,
				     PRI_D_ABBREV PRI_ABBR_ATTR,
				     "attribute form",
				     abbr_off, attr_off))
	    goto free_and_out;

	  null_attrib = attrib_name == 0 && attrib_form == 0;

	  /* Now if both are zero, this was the last attribute.  */
	  if (!null_attrib)
	    {
	      /* Otherwise validate name and form.  */
	      if (attrib_name > DW_AT_hi_user)
		{
		  wr_error (PRI_D_ABBREV PRI_ABBR_ATTR
			    ": invalid name 0x%" PRIx64 ".\n",
			    abbr_off, attr_off, attrib_name);
		  goto free_and_out;
		}

	      if (!attrib_form_valid (attrib_form))
		{
		  wr_error (PRI_D_ABBREV PRI_ABBR_ATTR
			    ": invalid form 0x%" PRIx64 ".\n",
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
		wr_error (PRI_D_ABBREV PRI_ABBR_ATTR
			  ": Another DW_AT_sibling attribute in one abbreviation. "
			  "(First was 0x%" PRIx64 ".)\n",
			  abbr_off, attr_off, sibling_attr);
	      else
		{
		  assert (attr_off > 0);
		  sibling_attr = attr_off;

		  if (!cur->has_children)
		    message (mc_die_rel_sib | mc_acc_bloat | mc_impact_1,
			     PRI_D_ABBREV PRI_ABBR_ATTR
			     ": Excessive DW_AT_sibling attribute at childless abbrev.\n",
			     abbr_off, attr_off);
		}

	      switch (check_sibling_form (attrib_form))
		{
		case -1:
		  message (mc_die_rel_sib | mc_impact_2,
			   PRI_D_ABBREV PRI_ABBR_ATTR
			   ": DW_AT_sibling attribute with form DW_FORM_ref_addr.\n",
			   abbr_off, attr_off);
		  break;

		case -2:
		  wr_error (PRI_D_ABBREV PRI_ABBR_ATTR
			    ": DW_AT_sibling attribute with non-reference form \"%s\".\n",
			    abbr_off, attr_off, dwarf_form_string (attrib_form));
		};
	    }
	  /* Similar for DW_AT_location.  */
	  else if (is_location_attrib (attrib_name))
	    {
	      if (!check_abbrev_location_form (attrib_form))
		wr_error (PRI_D_ABBREV PRI_ABBR_ATTR
			  ": location attribute with invalid form \"%s\".\n",
			  abbr_off, attr_off, dwarf_form_string (attrib_form));
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
ref_record_add (struct ref_record *rr, uint64_t addr, uint64_t who)
{
  REALLOC (rr, refs);
  struct ref *ref = rr->refs + rr->size++;
  ref->addr = addr;
  ref->who = who;
}

static void
ref_record_free (struct ref_record *rr)
{
  if (rr != NULL)
    free (rr->refs);
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
  assert (ar);
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

static bool
coverage_is_covered (struct coverage *ar, uint64_t address)
{
  assert (ar);
  assert (address <= ar->size);

  uint64_t bi = address / coverage_emt_bits;
  uint8_t bb = address % coverage_emt_bits;
  coverage_emt_type bm = (coverage_emt_type)1 << (coverage_emt_bits - 1 - bb);
  return !!(ar->buf[bi] & bm);
}

static bool
coverage_pristine (struct coverage *ar, uint64_t begin, uint64_t length)
{
  for (uint64_t i = 0; i < length; ++i)
    if (coverage_is_covered (ar, begin + i))
      return false;
  return true;
}

static void
coverage_find_holes (struct coverage *ar,
		     void (*cb)(uint64_t begin, uint64_t end, void *user),
		     void *user)
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
      cb (begin, a - 1, user);
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
found_hole (uint64_t begin, uint64_t end, void *data)
{
  struct hole_info *info = (struct hole_info *)data;
  bool all_zeroes = true;
  for (uint64_t i = begin; i <= end; ++i)
    if (((char*)info->d_buf)[i] != 0)
      {
	all_zeroes = false;
	break;
      }

  if (all_zeroes)
    message_padding_0 (info->category, begin, end, info->section);
  else
    /* XXX: This is actually lying in case that the unreferenced
       portion is composed of sequences of zeroes and non-zeroes.  */
    message_padding_n0 (info->category, begin, end, info->section);
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
		      struct ref_record *die_refs)
{
  bool retval = true;
  for (size_t i = 0; i < die_refs->size; ++i)
    {
      struct ref *ref = die_refs->refs + i;
      if (!addr_record_has_addr (&cu->die_addrs, ref->addr))
	{
	  wr_error (PRI_D_INFO PRI_CU_DIE
		    ": unresolved reference to " PRI_DIE ".\n",
		    cu->offset, ref->who, ref->addr);
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
	    wr_error (PRI_D_INFO PRI_CU_DIE
		      ": unresolved (non-CU-local) reference to "
		      PRI_DIE ".\n",
		      it->offset, ref->who, ref->addr);
	    retval = false;
	  }
	else if (ref_cu == it)
	  message (mc_impact_2 | mc_acc_suboptimal | mc_die_rel_ref,
		   PRI_D_INFO PRI_CU_DIE
		   ": local reference to " PRI_DIE " formed as global.\n",
		   it->offset, ref->who, ref->addr);
      }

  return retval;
}

static bool
read_size_extra (struct read_ctx *ctx, uint32_t size32, uint64_t *sizep,
		 bool *dwarf_64p, const char *format, ...)
{
  bool retval = true;

  va_list ap;
  va_start (ap, format);
  char *buf = NULL;

  if (size32 == DWARF3_LENGTH_64_BIT)
    {
      if (!read_ctx_read_8ubyte (ctx, sizep))
	{
	  if (vasprintf (&buf, format, ap) < 0)
	    buf = NULL;
	  wr_error ("%s: can't read 64bit CU length.n", buf ?: fmterr);
	  retval = false;
	  goto out;
	}

      *dwarf_64p = true;
    }
  else if (size32 >= DWARF3_LENGTH_MIN_ESCAPE_CODE)
    {
      if (vasprintf (&buf, format, ap) < 0)
	buf = NULL;
      wr_error ("%s: unrecognized CU length escape value: %"
		PRIx32 ".n", buf ?: fmterr, size32);
      retval = false;
      goto out;
    }
  else
    *sizep = size32;

 out:
  va_end (ap);
  free (buf);
  return retval;
}

static bool __attribute__ ((format (printf, 3, 4)))
check_zero_padding (struct read_ctx *ctx,
		    enum message_category category,
		    const char *format, ...)
{
  assert (ctx->ptr != ctx->end);
  const unsigned char *save_ptr = ctx->ptr;
  while (!read_ctx_eof (ctx))
    if (*ctx->ptr++ != 0)
      {
	ctx->ptr = save_ptr;
	return false;
      }

  va_list ap;
  va_start (ap, format);
  vmessage_padding_0 (category,
		      (uint64_t)(save_ptr - ctx->begin),
		      (uint64_t)(ctx->end - ctx->begin),
		      format, ap);
  va_end (ap);
  return true;
}

static struct cu *
check_debug_info_structural (struct read_ctx *ctx,
			     struct abbrev_table *abbrev_chain,
			     Elf_Data *strings,
			     Elf_Data *loc)
{
  struct ref_record die_refs;
  memset (&die_refs, 0, sizeof (die_refs));

  struct cu *cu_chain = NULL;

  bool success = true;

  struct coverage strings_coverage_mem, *strings_coverage = NULL;
  if (strings != NULL && check_category (mc_strings))
    {
      coverage_init (&strings_coverage_mem, strings->d_size);
      strings_coverage = &strings_coverage_mem;
    }

  struct coverage loc_coverage_mem, *loc_coverage = NULL;
  struct addr_record loc_addrs_mem, *loc_addrs = NULL;
  if (loc != NULL && check_category (mc_loc))
    {
      coverage_init (&loc_coverage_mem, loc->d_size);
      loc_coverage = &loc_coverage_mem;
      memset (&loc_addrs_mem, 0, sizeof (loc_addrs_mem));
      loc_addrs = &loc_addrs_mem;
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
	  && check_zero_padding (ctx, mc_die_other,
				 PRI_D_INFO PRI_CU, cu_off))
	break;

      /* CU length.  */
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  wr_error (PRI_D_INFO PRI_CU ": can't read CU length.\n", cu_off);
	  success = false;
	  break;
	}
      if (size32 == 0 && check_zero_padding (ctx, mc_die_other,
					     PRI_D_INFO PRI_CU, cu_off))
	break;

      if (!read_size_extra (ctx, size32, &size, &dwarf_64,
			    PRI_D_INFO PRI_CU, cu_off))
	{
	  success = false;
	  break;
	}

      if (!read_ctx_need_data (ctx, size))
	{
	  wr_error (PRI_D_INFO PRI_CU ": section doesn't have enough data"
		    " to read CU of size %" PRIx64 ".\n", cu_off, size);
	  ctx->ptr = ctx->end;
	  success = false;
	  break;
	}

      const unsigned char *cu_end = ctx->ptr + size;
      cur->length = cu_end - cu_begin; // Length including the length field.

      /* version + debug_abbrev_offset + address_size */
      uint64_t cu_header_size = 2 + (dwarf_64 ? 8 : 4) + 1;
      if (size < cu_header_size)
	{
	  wr_error (PRI_D_INFO PRI_CU ": claimed length of %" PRIx64
		    " doesn't even cover CU header.\n", cu_off, size);
	  success = false;
	  break;
	}
      else
	{
	  /* Make CU context begin just before the CU length, so that DIE
	     offsets are computed correctly.  */
	  struct read_ctx cu_ctx;
	  if (!read_ctx_init_sub (&cu_ctx, ctx, cu_begin, cu_end))
	    {
	      wr_error (PRI_D_INFO PRI_CU PRI_NOT_ENOUGH, cu_off, "next CU");
	      success = false;
	      break;
	    }
	  cu_ctx.ptr = ctx->ptr;

	  if (!check_cu_structural (&cu_ctx, cur, abbrev_chain, strings, loc,
				    dwarf_64, &die_refs, loc_addrs,
				    strings_coverage, loc_coverage))
	    {
	      success = false;
	      break;
	    }
	  if (cu_ctx.ptr != cu_ctx.end
	      && !check_zero_padding (&cu_ctx, mc_die_other,
				      PRI_D_INFO PRI_CU, cu_off))
	    message_padding_n0 (mc_die_other,
				read_ctx_get_offset (ctx), size,
				PRI_D_INFO PRI_CU, cu_off);
	}

      ctx->ptr += size;
    }

  // Only check this if above we have been successful.
  if (success && ctx->ptr != ctx->end)
    message (mc_die_other | mc_impact_4,
	     ".debug_info: CU lengths don't exactly match Elf_Data contents.");

  bool references_sound = check_global_die_references (cu_chain);
  ref_record_free (&die_refs);

  if (strings_coverage != NULL)
    {
      if (success)
	coverage_find_holes (strings_coverage, found_hole,
			     &((struct hole_info)
			       {PRI_D_STR, mc_strings, strings->d_buf}));
      coverage_free (strings_coverage);
    }

  if (loc_coverage != NULL)
    {
      if (success)
	coverage_find_holes (loc_coverage, found_hole,
			     &((struct hole_info)
			       {PRI_D_LOC, mc_loc, loc->d_buf}));
      coverage_free (loc_coverage);
    }

  if (loc_addrs != NULL)
    addr_record_free (loc_addrs);

  if (!success || !references_sound)
    {
      cu_free (cu_chain);
      cu_chain = NULL;
    }

  return cu_chain;
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
#define DEF_DW_OP(OPCODE, OP1, OP2)  \
      case OPCODE: *op1 = OP1; *op2 = OP2; return true;
# include "expr_opcodes.h"
#undef DEF_DW_OP
    default:
      return false;
    };
}

static bool
skip_form (struct read_ctx *ctx, bool addr_64, uint8_t form, ...)
{
  /* XXX fix messages in this function.  */
  va_list ap;
  va_start (ap, form);
  switch (form)
    {
    case DW_FORM_addr:
      return read_ctx_read_offset (ctx, addr_64, NULL);
    case DW_FORM_udata:
      return vchecked_read_uleb128 (ctx, NULL, "", NULL, ap); // here
    case DW_FORM_sdata:
      return vchecked_read_sleb128 (ctx, NULL, "", NULL, ap); // here
    case DW_FORM_data1:
      return read_ctx_read_ubyte (ctx, NULL);
    case DW_FORM_data2:
      return read_ctx_read_2ubyte (ctx, NULL);
    case DW_FORM_data4:
      return read_ctx_read_4ubyte (ctx, NULL);
    case DW_FORM_data8:
      return read_ctx_read_8ubyte (ctx, NULL);
    };

  return false;
}

/* If it returns false, an error has been written, and the caller
   should provide "at this point in file"-type message.  */
static bool
check_location_expression (struct read_ctx *ctx, bool addr_64)
{
  while (!read_ctx_eof (ctx))
    {
      uint64_t op_off = read_ctx_get_offset (ctx);
      uint8_t opcode;
      if (!read_ctx_read_ubyte (ctx, &opcode))
	{
	  wr_error (PRI_LOCEXPR PRI_RECORD ": can't read opcode.\n", op_off);
	  return false;
	}

      uint8_t op1, op2;
      if (!get_location_opcode_operands (opcode, &op1, &op2))
	{
	  wr_error (PRI_LOCEXPR PRI_RECORD ": can't decode opcode \"%s\".\n",
		    op_off, dwarf_locexpr_opcode_string (opcode));
	  return false;
	}

#define SKIP_FORM(OP, STR)						\
      if (OP != 0 && !skip_form (ctx, addr_64, OP))			\
	{								\
	  wr_error (PRI_LOCEXPR PRI_RECORD ": opcode \"%s\""		\
		    ": can't read " STR " operand (form \"%s\").\n",	\
		    op_off, dwarf_locexpr_opcode_string (opcode),	\
		    dwarf_form_string (OP));				\
	  return false;							\
	}

      SKIP_FORM (op1, "1st");
      SKIP_FORM (op2, "2nd");
#undef SKIP_FORM
    }

  return true;
}

/* If it returns false, an error has been written, and the caller
   should provide "at this point in file"-type message.  */
static bool
check_x_location_expression (Dwarf *dbg, Elf_Data *loc,
			     struct coverage *loc_coverage,
			     struct addr_record *loc_addrs,
			     uint64_t addr, bool addr_64)
{
  if (loc == NULL || loc_coverage == NULL)
    return true;

  struct read_ctx ctx;
  read_ctx_init (&ctx, dbg, loc);
  if (!read_ctx_skip (&ctx, addr))
    {
      wr_error ("invalid reference outside " PRI_D_LOC
		"0x%" PRIx64 ", size only 0x%" PRIx64 ".\n",
		addr, loc->d_size);
      return false;
    }

  if (coverage_is_covered (loc_coverage, addr))
    {
      if (!addr_record_has_addr (loc_addrs, addr))
	{
	  wr_error (PRI_D_LOC "0x%" PRIx64
		    ": reference to the middle of location list.\n", addr);
	  return false;
	}
      return true;
    }
  else
    addr_record_add (loc_addrs, addr);

  uint64_t escape = addr_64 ? (uint64_t)-1 : (uint64_t)(uint32_t)-1;

  bool retval = true;
  bool overlap = false;
  while (!read_ctx_eof (&ctx))
    {
      uint64_t off = read_ctx_get_offset (&ctx);

#define HAVE_OVERLAP						\
      do {							\
	wr_error (PRI_D_LOC PRI_RECORD				\
		  ": range definitions overlap.\n", off);	\
	retval = false;						\
	overlap = true;						\
      } while (0)

      /* begin address */
      uint64_t begin_addr;
      if (!overlap
	  && !coverage_pristine (loc_coverage,
				 read_ctx_get_offset (&ctx),
				 addr_64 ? 8 : 4))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, addr_64, &begin_addr))
	{
	  wr_error (PRI_D_LOC PRI_RECORD
		    ": can't read address range beginning.\n", off);
	  return false;
	}

      /* end address */
      uint64_t end_addr;
      if (!overlap
	  && !coverage_pristine (loc_coverage,
				 read_ctx_get_offset (&ctx),
				 addr_64 ? 8 : 4))
	HAVE_OVERLAP;

      if (!read_ctx_read_offset (&ctx, addr_64, &end_addr))
	{
	  wr_error (PRI_D_LOC PRI_RECORD
		    ": can't read address range ending.\n", off);
	  return false;
	}

      bool done = begin_addr == 0 && end_addr == 0;

      if (!done && begin_addr != escape)
	{
	  /* location expression length */
	  uint16_t len;
	  if (!overlap
	      && !coverage_pristine (loc_coverage,
				     read_ctx_get_offset (&ctx), 2))
	    HAVE_OVERLAP;

	  if (!read_ctx_read_2ubyte (&ctx, &len))
	    {
	      wr_error (PRI_D_LOC PRI_RECORD
			": can't read length of location expression.\n", off);
	      return false;
	    }

	  /* location expression itself */
	  struct read_ctx expr_ctx;
	  if (!read_ctx_init_sub (&expr_ctx, &ctx, ctx.ptr, ctx.ptr + len))
	    {
	    not_enough:
	      wr_error (PRI_D_LOC PRI_RECORD PRI_NOT_ENOUGH,
			off, "location expression");
	      return false;
	    }

	  uint64_t expr_start = read_ctx_get_offset (&ctx);
	  if (!check_location_expression (&expr_ctx, addr_64))
	    {
	      wr_error (PRI_D_LOC PRI_RECORD PRI_NOT_ENOUGH,
			off, "location expression");
	      retval = false;
	    }
	  uint64_t expr_end = read_ctx_get_offset (&ctx);
	  if (!overlap
	      && !coverage_pristine (loc_coverage,
				     expr_start, expr_end - expr_start))
	    HAVE_OVERLAP;

	  if (!read_ctx_skip (&ctx, len))
	    /* "can't happen" */
	    goto not_enough;
	}
#undef HAVE_OVERLAP

      uint64_t fin_off = read_ctx_get_offset (&ctx);
      coverage_add (loc_coverage, off, fin_off - 1);
      if (done)
	break;
    }

  return retval;
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
		struct abbrev_table *abbrevs,
		Elf_Data *strings,
		Elf_Data *loc,
		bool dwarf_64, bool addr_64,
		struct ref_record *die_refs,
		struct ref_record *die_loc_refs,
		struct addr_record *loc_addrs,
		struct coverage *strings_coverage,
		struct coverage *loc_coverage)
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
      if (!checked_read_uleb128 (ctx, &abbr_code,
				 PRI_D_INFO PRI_CU_DIE, "abbrev code",
				 cu->offset, die_off))
	return -1;

      /* Check sibling value advertised last time through the loop.  */
      if (sibling_addr != 0)
	{
	  if (abbr_code == 0)
	    wr_error (PRI_D_INFO PRI_CU_DIE
		   ": is the last sibling in chain, but has a DW_AT_sibling attribute.\n",
		   cu->offset, prev_die_off);
	  else if (sibling_addr != die_off)
	    wr_error (PRI_D_INFO PRI_CU_DIE
		   ": This DIE should have had its sibling at 0x%"
		   PRIx64 ", but it's at 0x%" PRIx64 " instead.\n",
		   cu->offset, prev_die_off, sibling_addr, die_off);
	  sibling_addr = 0;
	}
      else if (prev_abbrev != NULL && prev_abbrev->has_children)
	/* Even if it has children, the DIE can't have a sibling
	   attribute if it's the last DIE in chain.  That's the reason
	   we can't simply check this when loading abbrevs.  */
	message (mc_die_rel_sib | mc_acc_suboptimal | mc_impact_4,
		 PRI_D_INFO PRI_CU_DIE
		 ": This DIE had children, but no DW_AT_sibling attribute.\n",
		 cu->offset, prev_die_off);

      /* The section ended.  */
      if (read_ctx_eof (ctx) || abbr_code == 0)
	{
	  if (abbr_code != 0)
	    wr_error (PRI_D_INFO PRI_CU
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
	  wr_error (PRI_D_INFO PRI_CU_DIE ": abbrev section at 0x%" PRIx64
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

	  void record_ref (uint64_t addr, uint64_t who, bool local)
	  {
	    struct ref_record *record = &cu->die_refs;
	    if (local)
	      {
		assert (ctx->end > ctx->begin);
		if (addr > (uint64_t)(ctx->end - ctx->begin))
		  {
		    wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			   ": invalid reference outside the CU: 0x%" PRIx64 ".\n",
			   cu->offset, die_off, abbrev->code, it->offset, addr);
		    return;
		  }

		/* Address holds a CU-local reference, so add CU
		   offset to turn it into section offset.  */
		addr += cu->offset;
		record = die_loc_refs;
	      }

	    if (record != NULL)
	      ref_record_add (record, addr, who);
	  }

	  uint8_t form = it->form;
	  bool indirect = form == DW_FORM_indirect;
	  if (indirect)
	    {
	      uint64_t value;
	      if (!checked_read_uleb128 (ctx, &value,
					 PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					 "indirect attribute form",
					 cu->offset, die_off, abbrev->code,
					 it->offset))
		return -1;

	      if (!attrib_form_valid (value))
		{
		  wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			    ": invalid indirect form 0x%" PRIx64 ".\n",
			    cu->offset, die_off, abbrev->code, it->offset, value);
		  return -1;
		}
	      form = value;

	      if (it->name == DW_AT_sibling)
		switch (check_sibling_form (form))
		  {
		  case -1:
		    message (mc_die_rel_sib | mc_impact_2,
			     PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			     ": DW_AT_sibling attribute with (indirect) form DW_FORM_ref_addr.\n",
			     cu->offset, die_off, abbrev->code, it->offset);
		    break;

		  case -2:
		    wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			      ": DW_AT_sibling attribute with non-reference (indirect) form \"%s\".\n",
			      cu->offset, die_off, abbrev->code, it->offset,
			      dwarf_form_string (value));
		  };
	    }

	  bool check_locptr = false;
	  bool locptr_64 = addr_64;
	  if (is_location_attrib (it->name))
	    {
	      switch (check_CU_location_form (form, dwarf_64))
		{
		case 0: /* absolutely invalid */
		  /* Only print error if it's indirect.  Otherwise we
		     gave diagnostic during abbrev loading.  */
		  if (indirect)
		    wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			      ": location attribute with invalid (indirect) form \"%s\".\n",
			      cu->offset, die_off, abbrev->code, it->offset,
			      dwarf_form_string (form));
		  break;

		case -1: /* locptr invalid in this context */
		  wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			    ": location attribute with form \"%s\" in %d-bit CU.\n",
			    cu->offset, die_off, abbrev->code, it->offset,
			    dwarf_form_string (form), (dwarf_64 ? 64 : 32));
		  locptr_64 = !locptr_64;

		  /* fall-through */
		case 1: /* locptr */
		  check_locptr = true;
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
		    wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			   ": can't read attribute value.\n",
			   cu->offset, die_off, abbrev->code, it->offset);
		    return -1;
		  }

		if (strings == NULL)
		  wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
			 ": strp attribute, but no .debug_str section.\n",
			 cu->offset, die_off, abbrev->code, it->offset);
		else if (addr >= strings->d_size)
		  wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
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
		  record_ref (addr, die_off, false);

		/* XXX What are validity criteria for DW_FORM_addr? */
		break;
	      }

	    case DW_FORM_udata:
	    case DW_FORM_ref_udata:
	      {
		uint64_t value;
		if (!checked_read_uleb128 (ctx, &value,
					   PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					   "attribute value",
					   cu->offset, die_off, abbrev->code,
					   it->offset))
		  return -1;

		if (it->name == DW_AT_sibling)
		  sibling_addr = value;
		else if (it->form == DW_FORM_ref_udata)
		  record_ref (value, die_off, true);
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
		  record_ref (value, die_off, true);
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
		  record_ref (value, die_off, true);
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
		else if (check_locptr)
		  {
		    if (!check_x_location_expression (ctx->dbg, loc,
						      loc_coverage, loc_addrs,
						      value, locptr_64))
		      wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR PRI_CAUSE,
				cu->offset, die_off, abbrev->code, it->offset);
		  }
		else if (it->form == DW_FORM_ref4)
		  record_ref (value, die_off, true);
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
		else if (check_locptr)
		  {
		    if (!check_x_location_expression (ctx->dbg, loc,
						      loc_coverage, loc_addrs,
						      value, locptr_64))
		      wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR PRI_CAUSE,
				cu->offset, die_off, abbrev->code,
				it->offset);
		  }
		else if (it->form == DW_FORM_ref8)
		  record_ref (value, die_off, true);
		break;
	      }

	    case DW_FORM_sdata:
	      {
		int64_t value;
		if (!checked_read_sleb128 (ctx, &value,
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
		    if (!checked_read_uleb128 (ctx, &length,
					       PRI_D_INFO PRI_CU_DIE_ABBR_ATTR,
					       "attribute value",
					       cu->offset, die_off, abbrev->code,
					       it->offset))
		      return -1;
		  }
		else if (!read_ctx_read_var (ctx, width, &length))
		  goto cant_read;

		if (is_location_attrib (it->name))
		  {
		    struct read_ctx sub_ctx;
		    if (!read_ctx_init_sub (&sub_ctx, ctx, ctx->ptr,
					    ctx->ptr + length))
		      {
			wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR PRI_NOT_ENOUGH,
				  cu->offset, die_off, abbrev->code, it->offset,
				  "location expression");
			return -1;
		      }

		    if (!check_location_expression (&sub_ctx, addr_64))
		      wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR PRI_CAUSE,
				cu->offset, die_off, abbrev->code, it->offset);
		  }

		if (!read_ctx_skip (ctx, length))
		  goto cant_read;

		break;
	      }

	    case DW_FORM_indirect:
	      wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
		     ": Indirect form is again indirect.\n",
		     cu->offset, die_off, abbrev->code, it->offset);
	      return -1;

	    default:
	      wr_error (PRI_D_INFO PRI_CU_DIE_ABBR_ATTR
		     ": Internal error: unhandled form 0x%x\n",
		     cu->offset, die_off, abbrev->code, it->offset, it->form);
	    }
	}

      if (abbrev->has_children)
	{
	  int st = read_die_chain (ctx, cu, abbrevs, strings, loc,
				   dwarf_64, addr_64,
				   die_refs, die_loc_refs, loc_addrs,
				   strings_coverage, loc_coverage);
	  if (st == -1)
	    return -1;
	  else if (st == 0)
	    message (mc_impact_3 | mc_acc_suboptimal | mc_die_rel_child,
		     PRI_D_INFO PRI_CU_DIE
		     ": Abbrev has_children, but the chain was empty.\n",
		     cu->offset, die_off);
	}
    }

  if (sibling_addr != 0)
    wr_error (PRI_D_INFO PRI_CU_DIE
	   ": This DIE should have had its sibling at 0x%"
	   PRIx64 ", but the DIE chain ended.\n",
	   cu->offset, prev_die_off, sibling_addr);

  return got_die ? 1 : 0;
}

static bool
read_version (struct read_ctx *ctx, bool dwarf_64,
	      uint16_t *versionp, const char *format, ...)
{
  bool retval = read_ctx_read_2ubyte (ctx, versionp);
  char *buf = NULL;
  va_list ap;
  va_start (ap, format);

  if (!retval
      || *versionp < 2 || *versionp > 3
      || (*versionp == 2 && dwarf_64))
    if (vasprintf (&buf, format, ap) < 0)
      buf = NULL;

  if (!retval)
    {
      wr_error ("%s: can't read version.\n", buf ?: fmterr);
      retval = false;
      goto out;
    }

  if (*versionp < 2 || *versionp > 3)
    {
      wr_error ("%s: %s version %d.\n", buf ?: fmterr,
		(*versionp < 2 ? "invalid" : "unsupported"),
		*versionp);
      retval = false;
      goto out;
    }

  if (*versionp == 2 && dwarf_64)
    /* Keep going.  It's a standard violation, but we may still be
       able to read the unit under consideration and do high-level
       checks.  */
    wr_error ("%s: invalid 64-bit unit in DWARF 2 format.\n", buf ?: fmterr);

 out:
  va_end (ap);
  free (buf);
  return retval;
}

static bool
check_cu_structural (struct read_ctx *ctx,
		     struct cu *const cu,
		     struct abbrev_table *abbrev_chain,
		     Elf_Data *strings,
		     Elf_Data *loc,
		     bool dwarf_64,
		     struct ref_record *die_refs,
		     struct addr_record *loc_addrs,
		     struct coverage *strings_coverage,
		     struct coverage *loc_coverage)
{
  uint16_t version;
  uint64_t abbrev_offset;
  uint8_t address_size;

  /* Version.  */
  if (!read_version (ctx, dwarf_64, &version, PRI_D_INFO PRI_CU, cu->offset))
    return false;

  /* Abbrev offset.  */
  if (!read_ctx_read_offset (ctx, dwarf_64, &abbrev_offset))
    {
      wr_error (PRI_D_INFO PRI_CU ": can't read abbrev offset.\n", cu->offset);
      return false;
    }

  /* Address size.  */
  if (!read_ctx_read_ubyte (ctx, &address_size))
    {
      wr_error (PRI_D_INFO PRI_CU ": can't read address size.\n", cu->offset);
      return false;
    }
  if (address_size != 4 && address_size != 8)
    {
      wr_error (PRI_D_INFO PRI_CU
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
      wr_error (PRI_D_INFO PRI_CU
		": Couldn't find abbrev section with offset 0x%" PRIx64 ".\n",
		cu->offset, abbrev_offset);
      return false;
    }

  struct ref_record die_loc_refs;
  memset (&die_loc_refs, 0, sizeof (die_loc_refs));

  bool retval = true;
  if (read_die_chain (ctx, cu, abbrevs, strings, loc,
		      dwarf_64, address_size == 8,
		      die_refs, &die_loc_refs,
		      loc_addrs,
		      strings_coverage,
		      loc_coverage) >= 0)
    {
      for (size_t i = 0; i < abbrevs->size; ++i)
	if (!abbrevs->abbr[i].used)
	  message (mc_impact_3 | mc_acc_bloat | mc_abbrevs,
		   PRI_D_INFO PRI_CU ": Abbreviation with code %"
		   PRIu64 " is never used.\n",
		   cu->offset, abbrevs->abbr[i].code);

      if (!check_die_references (cu, &die_loc_refs))
	retval = false;
    }
  else
    retval = false;

  ref_record_free (&die_loc_refs);
  return retval;
}


static bool
check_aranges_structural (struct read_ctx *ctx, struct cu *cu_chain)
{
  bool retval = true;

  while (!read_ctx_eof (ctx))
    {
      uint64_t atab_off = read_ctx_get_offset (ctx);
      const unsigned char *atab_begin = ctx->ptr;

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      bool dwarf_64;
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB
		 ": can't read table length.\n", atab_off);
	  return false;
	}
      if (!read_size_extra (ctx, size32, &size, &dwarf_64,
			    PRI_D_ARANGES PRI_ARANGETAB, atab_off))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *atab_end = ctx->ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, ctx, atab_begin, atab_end))
	{
	not_enough:
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB PRI_NOT_ENOUGH,
		    atab_off, "next table");
	  return false;
	}

      sub_ctx.ptr = ctx->ptr;

      /* Version.  */
      uint16_t version;
      if (!read_version (&sub_ctx, dwarf_64, &version,
			 PRI_D_ARANGES PRI_ARANGETAB, atab_off))
	{
	  retval = false;
	  goto next;
	}

      /* CU offset.  */
      uint64_t cu_off;
      if (!read_ctx_read_offset (&sub_ctx, dwarf_64, &cu_off))
	{
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB
		    ": can't read debug info offset.\n", atab_off);
	  retval = false;
	  goto next;
	}
      if (cu_chain != NULL && cu_find_cu (cu_chain, cu_off) == NULL)
	wr_error (PRI_D_ARANGES PRI_ARANGETAB
		  ": unresolved reference to " PRI_CU ".\n", atab_off, cu_off);

      /* Address size.  */
      uint8_t address_size;
      if (!read_ctx_read_ubyte (&sub_ctx, &address_size))
	{
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU
		    ": can't read unit address size.\n", atab_off, cu_off);
	  retval = false;
	  goto next;
	}
      if (address_size != 2
	  && address_size != 4
	  && address_size != 8)
	{
	  /* XXX Does anyone need e.g. 6 byte addresses?  */
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU
		    ": invalid address size: %d.\n",
		    atab_off, cu_off, address_size);
	  retval = false;
	  goto next;
	}

      /* Segment size.  */
      uint8_t segment_size;
      if (!read_ctx_read_ubyte (&sub_ctx, &segment_size))
	{
	  wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU
		    ": can't read unit segment size.\n", atab_off, cu_off);
	  retval = false;
	  goto next;
	}
      if (segment_size != 0)
	{
	  wr_warning (PRI_D_ARANGES PRI_ARANGETAB_CU
		      ": dwarflint can't handle segment_size != 0.\n",
		      atab_off, cu_off);
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
		  wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU
			 ": section ends after the header, but before the first entry.\n",
			 atab_off, cu_off);
		  retval = false;
		  goto next;
		}
	      if (c != 0)
		message (mc_impact_2 | mc_aranges,
			 PRI_D_ARANGES PRI_ARANGETAB_CU
			 ": non-zero byte at 0x%" PRIx64
			 " in padding before the first entry.\n",
			 atab_off, cu_off, read_ctx_get_offset (&sub_ctx));
	    }
	}
      assert ((read_ctx_get_offset (&sub_ctx) % tuple_size) == 0);

      while (!read_ctx_eof (&sub_ctx))
	{
	  uint64_t tuple_off = read_ctx_get_offset (&sub_ctx);
	  uint64_t address, length;
	  if (!read_ctx_read_var (&sub_ctx, address_size, &address))
	    {
	      wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU_RECORD
		     ": can't read address field.\n",
		     atab_off, cu_off, tuple_off);
	      retval = false;
	      goto next;
	    }
	  if (!read_ctx_read_var (&sub_ctx, address_size, &length))
	    {
	      wr_error (PRI_D_ARANGES PRI_ARANGETAB_CU_RECORD
		     ": can't read length field.\n",
		     atab_off, cu_off, tuple_off);
	      retval = false;
	      goto next;
	    }

	  if (address == 0 && length == 0)
	    break;

	  /* Address and length can be validated on high level.  */
	}

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_pubtables,
				  PRI_D_ARANGES PRI_ARANGETAB_CU,
				  atab_off, cu_off))
	{
	  message_padding_n0 (mc_pubtables | mc_error,
			      read_ctx_get_offset (&sub_ctx), size,
			      PRI_D_ARANGES PRI_ARANGETAB_CU, atab_off, cu_off);
	  retval = false;
	}

    next:
      if (!read_ctx_skip (ctx, size))
	/* A "can't happen" error.  */
	goto not_enough;
    }

  return retval;
}

static bool
check_pub_structural (struct read_ctx *ctx, struct cu *cu_chain,
		      const char *secname)
{
  bool retval = true;

  while (!read_ctx_eof (ctx))
    {
      uint64_t set_off = read_ctx_get_offset (ctx);
      const unsigned char *set_begin = ctx->ptr;

      /* Size.  */
      uint32_t size32;
      uint64_t size;
      bool dwarf_64;
      if (!read_ctx_read_4ubyte (ctx, &size32))
	{
	  wr_error ("%s" PRI_PUBSET
		    ": can't read set length.\n", secname, set_off);
	  return false;
	}
      if (!read_size_extra (ctx, size32, &size, &dwarf_64,
			    "%s" PRI_PUBSET, secname, set_off))
	return false;

      struct read_ctx sub_ctx;
      const unsigned char *set_end = ctx->ptr + size;
      if (!read_ctx_init_sub (&sub_ctx, ctx, set_begin, set_end))
	{
	  wr_error ("%s" PRI_PUBSET PRI_NOT_ENOUGH,
		    secname, set_off, "next set");
	  return false;
	}
      sub_ctx.ptr = ctx->ptr;

      /* Version.  */
      uint16_t version;
      if (!read_ctx_read_2ubyte (&sub_ctx, &version))
	{
	  wr_error ("%s" PRI_PUBSET
		    ": can't read set version.\n", secname, set_off);
	  retval = false;
	  goto next;
	}
      if (version != 2)
	{
	  wr_error ("%s" PRI_PUBSET ": %s set version.\n", secname, set_off,
		    (version < 2 ? "invalid" : "unsupported"));
	  retval = false;
	  goto next;
	}

      /* CU offset.  */
      uint64_t cu_off;
      if (!read_ctx_read_offset (&sub_ctx, dwarf_64, &cu_off))
	{
	  wr_error ("%s" PRI_PUBSET
		    ": can't read debug info offset.\n", secname, set_off);
	  retval = false;
	  goto next;
	}
      struct cu *cu = cu_find_cu (cu_chain, cu_off);
      if (cu_chain != NULL && cu == NULL)
	wr_error ("%s" PRI_PUBSET ": unresolved reference to " PRI_CU ".\n",
		  secname, set_off, cu_off);

      /* Covered length.  */
      uint64_t cu_len;
      if (!read_ctx_read_offset (&sub_ctx, dwarf_64, &cu_len))
	{
	  wr_error ("%s" PRI_PUBSET_CU ": can't read debug info offset.\n",
		    secname, set_off, cu_off);
	  retval = false;
	  goto next;
	}
      if (cu_chain != NULL && cu_len != cu->length)
	{
	  wr_error ("%s" PRI_PUBSET_CU
		    ": the set covers length %" PRId64
		    " but CU has length %" PRId64 ".\n",
		    secname, set_off, cu_off, cu_len, cu->length);
	  retval = false;
	  goto next;
	}

      /* followed by a null-terminated character string
	 representing the name of the object as given by the
	 DW_AT_name attribute of the referenced debugging entry. Each
	 set of names is terminated by an offset field containing zero
	 (and no following string). */
      while (!read_ctx_eof (&sub_ctx))
	{
	  uint64_t pair_off = read_ctx_get_offset (&sub_ctx);
	  uint64_t offset;
	  if (!read_ctx_read_offset (&sub_ctx, dwarf_64, &offset))
	    {
	      wr_error ("%s" PRI_PUBSET_CU_RECORD
			": can't read offset field.\n",
			secname, set_off, cu_off, pair_off);
	      retval = false;
	      goto next;
	    }
	  if (offset == 0)
	    break;

	  if (cu_chain != NULL
	      && !addr_record_has_addr (&cu->die_addrs, offset + cu->offset))
	    {
	      wr_error ("%s" PRI_PUBSET_CU_RECORD
			": unresolved reference to " PRI_DIE ".\n",
			secname, set_off, cu_off, pair_off, offset);
	      retval = false;
	      goto next;
	    }

	  uint8_t c;
	  do
	    if (!read_ctx_read_ubyte (&sub_ctx, &c))
	      {
		wr_error ("%s" PRI_PUBSET_CU_RECORD
			  ": can't read symbol name.\n",
			  secname, set_off, cu_off, pair_off);
		retval = false;
		goto next;
	      }
	  while (c);
	}

      if (sub_ctx.ptr != sub_ctx.end
	  && !check_zero_padding (&sub_ctx, mc_pubtables,
				  "%s" PRI_PUBSET, secname, set_off))
	{
	  message_padding_n0 (mc_pubtables | mc_error,
			      read_ctx_get_offset (&sub_ctx), size,
			      "%s" PRI_PUBSET, secname, set_off);
	  retval = false;
	}

    next:
      ctx->ptr += size;
    }

  return retval;
}
