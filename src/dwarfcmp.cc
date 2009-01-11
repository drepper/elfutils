/* Compare semantic content of two DWARF files.
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

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <locale.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdint>
#include "../libdw/libdwP.h"	// XXX

#include "c++/dwarf"

using namespace elfutils;
using namespace std;


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

/* Values for the parameters which have no short form.  */
#define OPT_XXX			0x100

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Control options:"), 0 },
  { "quiet", 'q', NULL, 0, N_("Output nothing; yield exit status only"), 0 },
  { "ignore-missing", 'i', NULL, 0,
    N_("Don't complain if both files have no DWARF at all"), 0 },

  { "test-writer", 'T', NULL, 0, N_("Test DWARF output classes"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Compare two DWARF files for semantic equality.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE1 FILE2");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

/* Nonzero if only exit status is wanted.  */
static bool quiet;

/* Nonzero if missing DWARF is equal DWARF.  */
static bool missing_ok;

/* Nonzero to test writer classes.  */
static bool test_writer;


static Dwarf *
open_file (const char *fname, int *fdp)
{
  int fd = open (fname, O_RDONLY);
  if (unlikely (fd == -1))
    error (2, errno, gettext ("cannot open '%s'"), fname);
  Dwarf *dw = dwarf_begin (fd, DWARF_C_READ);
  *fdp = fd;
  if (dw == NULL)
    {
      int code = dwarf_errno ();
      if (code != DWARF_E_NO_DWARF || !missing_ok)
	error (2, 0,
	       gettext ("cannot create DWARF descriptor for '%s': %s"),
	       fname, dwarf_errmsg (code));
    }
  return dw;
}


// XXX make translation-friendly
struct context
{
  const dwarf::debug_info_entry *a_;
  const dwarf::debug_info_entry *b_;
  const char *container_;

  context (const dwarf::debug_info_entry &a, const dwarf::debug_info_entry &b)
    : a_ (&a), b_ (&b), container_ (NULL) {}
  context () : a_ (NULL), b_ (NULL), container_ ("compilation units") {}

  ostream &location () const
  {
    if (a_ == NULL)
      cout << "files differ: ";
    else
      cout << hex << a_->offset () << " vs " << b_->offset () << ": ";
    return cout;
  }

  void container (const char *msg) const
  {
    location () << msg << " " << container_ << endl;
  }

  void missing () const
  {
    container ("missing");
  }

  void extra () const
  {
    container ("extra");
  }

  void tag () const
  {
    location () << "different tag" << endl;
  }

  void attributes () const
  {
    location () << "different attributes" << endl;
  }

  void values (int name) const
  {
    location () << "different values for attribute 0x" << hex << name << endl;
  }
};

template<typename container1, typename container2>
static int
describe_mismatch (const container1 &a, const container2 &b, const context &say)
{
  typename container1::const_iterator i = a.begin ();
  typename container2::const_iterator j = b.begin ();
  int result = 0;
  while (i != a.end ())
    {
      if (j == b.end ())
	{
	  say.missing ();	// b lacks some of a.
	  result = 1;
	  break;
	}
      result = describe_mismatch (*i, *j, say);
      assert ((result != 0) == (*i != *j));
      if (result != 0)
	break;
      ++i;
      ++j;
    }
  if (result == 0 && j != b.end ())
    {
      say.extra ();		// a lacks some of b.
      result = 1;
    }
  return result;
}

template<>
int
describe_mismatch (const dwarf::debug_info_entry &a,
		   const dwarf::debug_info_entry &b,
		   const context &ctx)
{
  context here (a, b);

  int result = a.tag () != b.tag ();
  if (result != 0)
    here.tag ();

  if (result == 0)
    {
      here.container_ = "attributes";
      result = describe_mismatch (a.attributes (), b.attributes (), here);
      assert ((result != 0) == (a.attributes () != b.attributes ()));
    }
  if (result == 0)
    {
      here.container_ = "children";
      result = describe_mismatch (a.children (), b.children (), here);
      assert ((result != 0) == (a.children () != b.children ()));
    }
  return result;
}

template<>
int
describe_mismatch (const dwarf::compile_unit &a, const dwarf::compile_unit &b,
		   const context &ctx)
{
  return describe_mismatch (static_cast<const dwarf::debug_info_entry &> (a),
			    static_cast<const dwarf::debug_info_entry &> (b),
			    ctx);
}

template<>
int
describe_mismatch (const dwarf::attribute &a, const dwarf::attribute &b,
		   const context &say)
{
  int result = a.first != b.first;
  if (result != 0)
    say.attributes ();
  else
    {
      result = a.second != b.second;
      if (result != 0)
	say.values (a.first);
    }
  return result;
}

int
main (int argc, char *argv[])
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* We expect exactly two non-option parameters.  */
  if (unlikely (remaining + 2 != argc))
    {
      fputs (gettext ("Invalid number of parameters.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE, program_invocation_short_name);
      exit (1);
    }
  const char *const fname1 = argv[remaining];
  int fd1;
  Dwarf *dw1 = open_file (fname1, &fd1);

  const char *const fname2 = argv[remaining + 1];
  int fd2;
  Dwarf *dw2 = open_file (fname2, &fd2);

  int result = 0;

  if (dw1 == NULL || dw2 == NULL)
    {
      result = (dw1 == NULL) != (dw2 == NULL);
      if (result != 0 && !quiet)
	{
	  if (dw1 == NULL)
	    cout << "unexpectedly has DWARF";
	  else
	    cout << "has no DWARF";
	}
    }
  else
    {
      dwarf file1 (dw1);
      dwarf file2 (dw2);

      if (quiet)
	result = !(file1 == file2);
      else
	result = describe_mismatch (file1.compile_units (),
				    file2.compile_units (),
				    context ());

      if (test_writer)
	{
	  dwarf_output out1 (file1);
	  dwarf_output out2 (file2);

# define compare_self(x, y)			\
	  assert (x == y);			\
	  assert (!(x != y))
# define compare_other(x, y)			\
	  assert (!(x == y) == result);		\
	  assert (!(x != y) == !result)

	  // Compare self, same type.
	  compare_self (out1, out1);
	  compare_self (out2, out2);

	  // Compare self, output == input.
	  compare_self (out1, file1);
	  compare_self (out2, file2);

	  // Compare self, input == output.
	  compare_self (file1, out1);
	  compare_self (file2, out2);

	  // Compare files, output == output.
	  compare_other (out1, out2);
	  compare_other (out2, out1);

	  // Compare files, output vs input.
	  compare_other (out1, file2);
	  compare_other (out2, file1);

	  // Compare files, input vs output.
	  compare_other (file2, out1);
	  compare_other (file1, out2);

#undef	compare_self
#undef	compare_other
	}
    }

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "dwarfcmp (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2009");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'q':
      quiet = true;
      break;

    case 'i':
      missing_ok = true;
      break;

    case 'T':
      test_writer = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
