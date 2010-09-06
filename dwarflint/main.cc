/* Main entry point for dwarflint, a pedantic checker for DWARF files.
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

#include <iostream>
#include <sstream>

#include "low.h"
#include "options.h"
#include "dwarflint.hh"
#include "readctx.h"
#include "checks.hh"

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

#define ARGP_strict	300
#define ARGP_gnu	301
#define ARGP_tolerant	302
#define ARGP_ref        303
#define ARGP_nohl       304
#define ARGP_dump_off   305
#define ARGP_check      306
#define ARGP_list_checks 307

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
  { "check", ARGP_check, "[+-][@]name,...", 0,
    N_("Only run selected checks."), 0 },
  { "list-checks", ARGP_list_checks, NULL, 0,
    N_("List all the available checks."), 0 },
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

/* Whether to list available checks and exit.  */
static bool just_list_checks = false;


static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

struct initial_check_rules
  : public check_rules
{
  initial_check_rules () {
    push_back (check_rule ("@all", check_rule::request));
    push_back (check_rule ("@nodefault", check_rule::forbid));
  }
} rules;

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

    case ARGP_check:
      {
	static bool first = true;
	std::stringstream ss (arg);
	std::string item;

    	while (std::getline (ss, item, ','))
	  {
	    if (item.empty ())
	      continue;

	    enum {
	      forbid,
	      request,
	      replace
	    } act;

	    // If the first rule has no operator, we assume the user
	    // wants to replace the implicit set of checks.
	    if (first)
	      {
		act = replace;
		first = false;
	      }
	    else
	      // Otherwise the rules are implicitly requesting, even
	      // without the '+' operator.
	      act = request;

	    bool minus = item[0] == '-';
	    bool plus = item[0] == '+';
	    if (plus || minus)
	      item = item.substr (1);
	    if (plus)
	      act = request;
	    if (minus)
	      act = forbid;

	    if (act == replace)
	      {
		rules.clear ();
		act = request;
	      }

	    check_rule::action_t action
	      = act == request ? check_rule::request : check_rule::forbid;
	    rules.push_back (check_rule (item, action));
	  }
      }
      break;

    case ARGP_list_checks:
      just_list_checks = true;
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
      if (!just_list_checks)
	{
	  fputs (gettext ("Missing file name.\n"), stderr);
	  argp_help (&argp, stderr, ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR,
		     program_invocation_short_name);
	  exit (1);
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
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

  if (just_list_checks)
    {
      dwarflint::check_registrar::inst ()->list_checks ();
      std::exit (0);
    }

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
      try
	{
	  char const *fname = argv[remaining];
	  /* Create an `Elf' descriptor.  */
	  unsigned int prev_error_count = error_count;
	  if (!only_one)
	    std::cout << std::endl << fname << ":" << std::endl;
	  dwarflint lint (fname, rules);

	  if (prev_error_count == error_count && !be_quiet)
	    puts (gettext ("No errors"));
	}
      catch (std::runtime_error &e)
	{
	  wr_error () << e.what () << std::endl;
	  continue;
	}
    }
  while (++remaining < argc);

  return error_count != 0;
}
