/* Pedantic checking of DWARF files.  Low-level checks.
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
#include <argp.h>
#include <libintl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <error.h>

#include <iostream>

#include "low.h"
#include "config.h"
#include "dwarflint.hh"
#include "readctx.h"
#include "checks.hh"
#include "checks-low.hh" // xxx

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

#define ARGP_strict	300
#define ARGP_gnu	301
#define ARGP_tolerant	302
#define ARGP_ref        303
#define ARGP_nohl       304
#define ARGP_dump_off   305

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
  { "tolerant", ARGP_tolerant, NULL, 0,
    N_("Don't output certain common error messages"), 0 },
  { "ref", ARGP_ref, NULL, 0,
    N_("When validating .debug_loc and .debug_ranges, display information about \
the DIE referring to the entry in consideration"), 0 },
  { "nohl", ARGP_nohl, NULL, 0,
    N_("Don't run high-level tests"), 0 },
  { "verbose", 'v', NULL, 0,
    N_("Be verbose"), 0 },
  { "dump-offsets", ARGP_dump_off, NULL, 0,
    N_("Dump DIE offsets to stderr as the tree is iterated."), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Pedantic checking of DWARF stored in ELF files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE...");

/* Messages that are accepted (and made into warning).  */
struct message_criteria warning_criteria;

/* Accepted (warning) messages, that are turned into errors.  */
struct message_criteria error_criteria;


static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

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

    case ARGP_tolerant:
      be_tolerant = true;
      break;

    case ARGP_ref:
      show_refs = true;
      break;

    case ARGP_nohl:
      do_high_level = false;
      break;

    case ARGP_dump_off:
      dump_die_offsets = true;
      break;

    case 'i':
      tolerate_nodebug = true;
      break;

    case 'q':
      be_quiet = true;
      be_verbose = false;
      break;

    case 'v':
      be_quiet = false;
      be_verbose = true;
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

static int
layout_rel_file (Elf *elf)
{
  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    return 1;

  if (ehdr.e_type != ET_REL)
    return 0;

  /* Taken from libdwfl. */
  GElf_Addr base = 0;
  GElf_Addr start = 0, end = 0, bias = 0;

  bool first = true;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (unlikely (shdr == NULL))
	return 1;

      if (shdr->sh_flags & SHF_ALLOC)
	{
	  const GElf_Xword align = shdr->sh_addralign ?: 1;
	  const GElf_Addr next = (end + align - 1) & -align;
	  if (shdr->sh_addr == 0
	      /* Once we've started doing layout we have to do it all,
		 unless we just layed out the first section at 0 when
		 it already was at 0.  */
	      || (bias == 0 && end > start && end != next))
	    {
	      shdr->sh_addr = next;
	      if (end == base)
		/* This is the first section assigned a location.
		   Use its aligned address as the module's base.  */
		start = base = shdr->sh_addr;
	      else if (unlikely (base & (align - 1)))
		{
		  /* If BASE has less than the maximum alignment of
		     any section, we eat more than the optimal amount
		     of padding and so make the module's apparent
		     size come out larger than it would when placed
		     at zero.  So reset the layout with a better base.  */

		  start = end = base = (base + align - 1) & -align;
		  Elf_Scn *prev_scn = NULL;
		  do
		    {
		      prev_scn = elf_nextscn (elf, prev_scn);
		      GElf_Shdr prev_shdr_mem;
		      GElf_Shdr *prev_shdr = gelf_getshdr (prev_scn,
							   &prev_shdr_mem);
		      if (unlikely (prev_shdr == NULL))
			return 1;
		      if (prev_shdr->sh_flags & SHF_ALLOC)
			{
			  const GElf_Xword prev_align
			    = prev_shdr->sh_addralign ?: 1;

			  prev_shdr->sh_addr
			    = (end + prev_align - 1) & -prev_align;
			  end = prev_shdr->sh_addr + prev_shdr->sh_size;

			  if (unlikely (! gelf_update_shdr (prev_scn,
							    prev_shdr)))
			    return 1;
			}
		    }
		  while (prev_scn != scn);
		  continue;
		}

	      end = shdr->sh_addr + shdr->sh_size;
	      if (likely (shdr->sh_addr != 0)
		  && unlikely (! gelf_update_shdr (scn, shdr)))
		return 1;
	    }
	  else
	    {
	      /* The address is already assigned.  Just track it.  */
	      if (first || end < shdr->sh_addr + shdr->sh_size)
		end = shdr->sh_addr + shdr->sh_size;
	      if (first || bias > shdr->sh_addr)
		/* This is the lowest address in the module.  */
		bias = shdr->sh_addr;

	      if ((shdr->sh_addr - bias + base) & (align - 1))
		/* This section winds up misaligned using BASE.
		   Adjust BASE upwards to make it congruent to
		   the lowest section address in the file modulo ALIGN.  */
		base = (((base + align - 1) & -align)
			+ (bias & (align - 1)));
	    }

	  first = false;
	}
    }
  return 0;
}

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

  /* Initialize warning & error criteria.  */
  warning_criteria |= message_term (mc_none, mc_none);

  error_criteria |= message_term (mc_impact_4, mc_none);
  error_criteria |= message_term (mc_error, mc_none);

  /* Configure warning & error criteria according to configuration.  */
  if (tolerate_nodebug)
    warning_criteria &= message_term (mc_none, mc_elf);

  if (be_gnu)
    warning_criteria &= message_term (mc_none, mc_acc_bloat);

  if (!be_strict)
    {
      warning_criteria &= message_term (mc_none, mc_strings);
      warning_criteria
	&= message_term (cat (mc_line, mc_header, mc_acc_bloat), mc_none);
      warning_criteria &= message_term (mc_none, mc_pubtypes);
    }

  if (be_tolerant)
    {
      warning_criteria &= message_term (mc_none, mc_loc);
      warning_criteria &= message_term (mc_none, mc_ranges);
    }

  if (be_verbose)
    {
      std::cout << "warning criteria: " << warning_criteria.str () << std::endl;
      std::cout << "error criteria:   " << error_criteria.str () << std::endl;
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
      Elf *elf = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
      if (elf == NULL)
      invalid_elf:
	wr_error (NULL,
		  gettext ("Error processing ELF file: %s\n"),
		  elf_errmsg (-1));
      else
	{
	  unsigned int prev_error_count = error_count;
	  if (layout_rel_file (elf))
	    goto invalid_elf;

	  if (!only_one)
	    std::cout << std::endl << argv[remaining] << ":" << std::endl;
	  dwarflint lint (elf);

	  elf_errno (); /* Clear errno.  */
	  elf_end (elf);
	  int err = elf_errno ();
	  if (err != 0)
	    wr_error (NULL,
		      gettext ("error while closing Elf descriptor: %s\n"),
		      elf_errmsg (err));

	  if (prev_error_count == error_count && !be_quiet)
	    puts (gettext ("No errors"));
	}

      close (fd);
    }
  while (++remaining < argc);

  return error_count != 0;
}
