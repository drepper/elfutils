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
#include "c++/dwarf_edit"
#include "c++/dwarf_comparator"
#include "c++/dwarf_tracker"
#include "c++/dwarf_output"

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
static int test_writer;


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
      if (code != DWARF_E_NO_DWARF
	  || (!missing_ok && (!test_writer || (exit (77), false))))
	error (2, 0,
	       gettext ("cannot create DWARF descriptor for '%s': %s"),
	       fname, dwarf_errmsg (code));
    }
  return dw;
}


// XXX make translation-friendly

template<class dwarf1, class dwarf2>
struct talker : public dwarf_ref_tracker<dwarf1, dwarf2>
{
  typedef dwarf_tracker_base<dwarf1, dwarf2> _base;
  typedef dwarf_ref_tracker<dwarf1, dwarf2> _tracker;
  typedef typename _base::cu1 cu1;
  typedef typename _base::cu2 cu2;
  typedef typename _base::die1 die1;
  typedef typename _base::die2 die2;
  typedef typename _base::dwarf1_die::attributes_type::const_iterator attr1;
  typedef typename _base::dwarf2_die::attributes_type::const_iterator attr2;

  const typename dwarf1::debug_info_entry *a_;
  const typename dwarf2::debug_info_entry *b_;

  inline talker ()
    : a_ (NULL), b_ (NULL)
  {}

  inline talker (const talker &proto, typename _tracker::reference_match &m,
		 const typename _tracker::left_context_type &l, const die1 &a,
		 const typename _tracker::right_context_type &r, const die2 &b)
    : _tracker (static_cast<const _tracker &> (proto), m, l, a, r, b),
      a_ (NULL), b_ (NULL)
  {
  }

  inline ostream &location () const
  {
    return cout << hex << a_->offset () << " vs " << b_->offset () << ": ";
  }

  inline void visit (const typename dwarf1::debug_info_entry &a,
		     const typename dwarf2::debug_info_entry &b)
  {
    a_ = &a;
    b_ = &b;
    if (a.tag () != b.tag ())
      location () << dwarf::tags::name (a.tag ())
		  << " vs "
		  << dwarf::tags::name (b.tag ());
  }

  inline void mismatch (const cu1 &it1, const cu1 &end1,
			const cu2 &it2, const cu2 &end2)
  {
    if (it1 == end1)		// a lacks some of b's CUs.
      cout << "files differ: "
	   << dec << subr::length (it2, end2)
	   << " extra compilation units "
	   << endl;
    else if (it2 == end2)	// b lacks some of a's CUs.
      cout << "files differ: "
	   << dec << subr::length (it1, end1)
	   << " compilation units missing "
	   << endl;
    // Otherwise the differing CU will have announced itself.
  }

  inline void mismatch (const die1 &it1, const die1 &end1,
			const die2 &it2, const die2 &end2)
  {
    if (it1 == end1)		// a_ lacks some of b_'s children.
      location () << dec << subr::length (it2, end2)
		  << " extra children " << endl;
    else if (it2 == end2)	// b_ lacks some of a_'s children.
      location () << dec << subr::length (it1, end1)
		  << " children missing " << endl;
    // Otherwise the differing child will have announced itself.
  }

  inline void mismatch (attr1 it1, const attr1 &end1,
			attr2 it2, const attr2 &end2)
  {
    if (it1 == end1)		// a_ lacks some of b_'s attrs.
      for (location () << " extra attributes:"; it2 != end2; ++it2)
	cout << " " << to_string (*it2);
    else if (it2 == end2)	// b_ lacks some of a_'s attrs.
      for (location () << " missing attributes:"; it1 != end1; ++it1)
	cout << " " << to_string (*it1);
    else
      location () << to_string (*it1) << " vs " << to_string (*it2);
    cout << endl;
  }
};

template<class dwarf1, class dwarf2, class tracker>
struct cmp
  : public dwarf_comparator<dwarf1, dwarf2, false, tracker>
{
  tracker _m_tracker;
  inline cmp ()
    : dwarf_comparator<dwarf1, dwarf2, false, tracker> (_m_tracker)
  {}
};

// For a silent comparison we just use the standard ref tracker.
template<class dwarf1, class dwarf2>
struct quiet_cmp
  : public cmp<dwarf1, dwarf2, dwarf_ref_tracker<dwarf1, dwarf2> >
{};

// To be noisy, the talker wraps the standard tracker with verbosity hooks.
template<class dwarf1, class dwarf2>
struct noisy_cmp
  : public cmp<dwarf1, dwarf2, talker<dwarf1, dwarf2> >
{};


// Test that one comparison works as expected.
template<class dwarf1, class dwarf2>
static void
test_compare (const dwarf1 &file1, const dwarf2 &file2, bool expect)
{
  if (quiet_cmp<dwarf1, dwarf2> () (file1, file2) != expect)
    {
      if (expect)
	noisy_cmp<dwarf1, dwarf2> () (file1, file2);
      throw std::logic_error (__PRETTY_FUNCTION__);
    }
}

// Test all directions of two classes.
template<class dwarf1, class dwarf2>
static void
test_classes (const dwarf1 &file1, const dwarf1 &file2,
	      const dwarf2 &out1, const dwarf2 &out2,
	      bool expect)
{
  // Compare self, same type.
  test_compare (out1, out1, true);
  test_compare (out2, out2, true);

  // Compare self, output == input.
  test_compare (out1, file1, true);
  test_compare (out2, file2, true);

  // Compare self, input == output.
  test_compare (file1, out1, true);
  test_compare (file2, out2, true);

  // Compare files, output == output.
  test_compare (out1, out2, expect);
  test_compare (out2, out1, expect);

  // Compare files, output vs input.
  test_compare (out1, file2, expect);
  test_compare (out2, file1, expect);

  // Compare files, input vs output.
  test_compare (file2, out1, expect);
  test_compare (file1, out2, expect);
}

template<class input>
static void
test_output (const dwarf &file1, const dwarf &file2,
	     bool two_tests, const input &in1, const input &in2, bool same)
{
  dwarf_output_collector c1;
  dwarf_output_collector c2;
  dwarf_output out1 (in1, c1);
  c1.stats ();
  dwarf_output out2 (in2, c2);
  c2.stats ();

  test_classes (file1, file2, out1, out2, same);

  if (two_tests)
    test_classes (in1, in2, out1, out2, same);
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

      bool same = (quiet
		   ? quiet_cmp<dwarf, dwarf> () (file1, file2)
		   : noisy_cmp<dwarf, dwarf> () (file1, file2));

      if (test_writer & 1)
	test_output (file1, file2, false, file1, file2, same);
      if (test_writer & 2)
	{
	  dwarf_edit edit1 (file1);
	  dwarf_edit edit2 (file2);
	  test_classes (file1, file2, edit1, edit2, same);
	  if (test_writer & 1)
	    test_output (file1, file2, true, edit1, edit2, same);
	}

      result = !same;
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
parse_opt (int key, char *, struct argp_state *)
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
      ++test_writer;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
