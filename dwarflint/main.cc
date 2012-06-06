/* Main entry point for dwarflint, a pedantic checker for DWARF files.
   Copyright (C) 2008,2009,2010,2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <libintl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <sstream>

#include "dwarflint.hh"
#include "readctx.hh"
#include "checks.hh"
#include "option.hh"
#include "messages.hh"

/* Messages that are accepted (and made into warning).  */
struct message_criteria warning_criteria;

/* Accepted (warning) messages, that are turned into errors.  */
struct message_criteria error_criteria;

struct check_option_t
  : public global_opt<option_common>
{
  struct initial_checkrules
    : public checkrules
  {
    initial_checkrules ()
    {
      push_back (checkrule_internal ("@all", checkrule::request));
      push_back (checkrule_internal ("@nodefault", checkrule::forbid));
    }
  } rules;

  check_option_t ()
    : global_opt<option_common> ("Only run selected checks.",
				 "[+-][@]name,...", "check", 0)
  {}

  error_t parse_opt (char *arg, __attribute__ ((unused)) argp_state *state)
  {
    static bool first = true;
    std::stringstream ss (arg);
    std::string item;

    while (std::getline (ss, item, ','))
      {
	if (item.empty ())
	  continue;

	enum
	{
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

	checkrule::action_t action
	  = act == request ? checkrule::request : checkrule::forbid;
	rules.push_back (checkrule (item, action));
      }
    return 0;
  }
} check_option;

global_opt<void_option>
  be_quiet ("Do not print anything if successful",
	    "quiet", 'q');

global_opt<string_option>
  opt_list_checks ("List all the available checks.",
		   "full", "list-checks", 0,
		   OPTION_ARG_OPTIONAL);

// xxx The following three should go away when we introduce the
// message filtering.  Or should be preserved, but in a way that makes
// more sense, right now they are simply a misnomer.
global_opt<void_option>
  ignore_bloat ("Ignore messages related to bloat.", "ignore-bloat");
global_opt<void_option>
  be_strict ("Be somewhat stricter.", "strict");
global_opt<void_option>
  be_tolerant ("Be somewhat more tolerant.", "tolerant");

int
main (int argc, char *argv[])
{
  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Initialize the message catalog.  */
  textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  argppp argp (global_opts (),
	       dwarflint::main_registrar ()->get_descriptors ());

  int remaining;
  argp.parse (argc, argv, 0, &remaining);

  if (opt_list_checks.seen ())
    {
      dwarflint::list_checks ();
      std::exit (0);
    }
  else if (remaining == argc)
    {
      fputs (gettext ("Missing file name.\n"), stderr);
      argp.help (stderr, ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR,
		 program_invocation_short_name);
      std::exit (1);
    }

  /* Initialize warning & error criteria.  */
  warning_criteria |= message_term (mc_none, mc_none);

  error_criteria |= message_term (mc_impact_4, mc_none);
  error_criteria |= message_term (mc_error, mc_none);

  /* Configure warning & error criteria according to configuration.  */
  if (ignore_bloat)
    warning_criteria &= message_term (mc_none, mc_acc_bloat);

  if (!be_strict)
    {
      warning_criteria &= message_term (mc_none, mc_strings);
      warning_criteria.and_not (mc_line | mc_acc_bloat);
      warning_criteria &= message_term (mc_none, mc_pubtypes);
    }

  if (be_tolerant)
    {
      warning_criteria &= message_term (mc_none, mc_loc);
      warning_criteria &= message_term (mc_none, mc_ranges);
    }

  if (false) // for debugging
    {
      std::cout << "warning criteria: " << warning_criteria << std::endl;
      std::cout << "error criteria:   " << error_criteria << std::endl;
    }

  /* Before we start tell the ELF library which version we are using.  */
  elf_version (EV_CURRENT);

  /* Now process all the files given at the command line.  */
  bool only_one = remaining + 1 == argc;
  bool one_passed = false;
  do
    {
      try
	{
	  char const *fname = argv[remaining];
	  if (!only_one)
	    std::cout << std::endl << fname << ":" << std::endl;
	  wr_reset_counters ();
	  dwarflint lint (fname, check_option.rules);
	  one_passed = true;

	  if (error_count == 0 && !be_quiet)
	    puts (gettext ("No errors"));
	}
      catch (std::runtime_error &e)
	{
	  wr_error () << e.what () << std::endl;
	  continue;
	}
    }
  while (++remaining < argc);

  if (one_passed)
    for (checkrules::const_iterator it = check_option.rules.begin ();
	 it != check_option.rules.end (); ++it)
      if (!it->used ())
	std::cerr << "warning: the rule `" << it->name ()
		  << "' never matched." << std::endl;

  return error_count != 0;
}
