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

#include <cinttypes>
#include "../libdw/libdwP.h"	// XXX

#include "c++/dwarf"
#include "c++/dwarf_comparator"
#include "c++/dwarf_tracker"

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
  { "verbose", 'l', NULL, 0, N_("Output all differences"), 0 },
  { "ignore-missing", 'i', NULL, 0,
    N_("Don't complain if both files have no DWARF at all"), 0 },

#ifdef TEST
  { "test-writer", 'T', NULL, 0, N_("Test DWARF output classes"), 0 },
#endif

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Compare two DWARF files for semantic equality.\n\
In the second form, compare only the given pair of entries.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE1 FILE2\n\
FILE1 FILE2 OFFSET1 OFFSET2");

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

/* True if we should print all differences.  */
static bool verbose;

#ifdef TEST
/* Nonzero to test writer classes.  */
static int test_writer;
#else
# define test_writer 0
#endif

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

template<class dwarf1, class dwarf2, bool print_all>
struct talker : public dwarf_ref_tracker<dwarf1, dwarf2>
{
  typedef dwarf_tracker_base<dwarf1, dwarf2> _base;
  typedef dwarf_ref_tracker<dwarf1, dwarf2> subtracker;
  typedef typename _base::cu1 cu1;
  typedef typename _base::cu2 cu2;
  typedef typename _base::die1 die1;
  typedef typename _base::die2 die2;
  typedef typename _base::dwarf1_die::attributes_type::const_iterator attr1;
  typedef typename _base::dwarf2_die::attributes_type::const_iterator attr2;

  template<typename die>
  struct die_hasher
    : public std::unary_function<die, dwarf::debug_info_entry::identity_type>
  {
    inline dwarf::debug_info_entry::identity_type
    operator () (const die &ref) const
    {
      return (*ref).identity ();
    }
  };

  template<typename die>
  struct die_equal_to
    : public std::equal_to<die>
  {
    inline dwarf::debug_info_entry::identity_type
    operator () (const die &a, const die &b) const
    {
      return (*a).identity () == (*b).identity ();
    }
  };

  typedef std::tr1::unordered_set<die2, die_hasher<die2>, die_equal_to<die2>
				  > die2_set;
  typedef std::tr1::unordered_map<die1, die2_set,
				  die_hasher<die1>, die_equal_to<die1>
				  > context_map;
  context_map bad_context_;
  context_map reference_mismatches_;
  typedef std::pair<dwarf::debug_info_entry::identity_type,
		    dwarf::debug_info_entry::identity_type> identity_pair;
  std::set<identity_pair> mismatches_printed_;

  string prefix_;

  const typename dwarf1::debug_info_entry *a_;
  const typename dwarf2::debug_info_entry *b_;
  bool result_;
  bool visiting_result_;
  bool mismatches_pending_;

  inline talker ()
    : a_ (NULL), b_ (NULL), result_ (true), mismatches_pending_ (false)
  {}

  inline talker (const talker &proto, typename subtracker::reference_match &m,
		 const typename subtracker::left_context_type &l,
		 const typename subtracker::right_context_type &r)
    : subtracker (static_cast<const subtracker &> (proto), m, l, r),
      a_ (NULL), b_ (NULL)
  {
  }

  inline ostream &location () const
  {
    return cout << prefix_
		<< hex << a_->offset () << " vs " << b_->offset ()
		<< ": ";
  }

  inline void visit (const typename dwarf1::debug_info_entry &a,
		     const typename dwarf2::debug_info_entry &b)
  {
    a_ = &a;
    b_ = &b;
    visiting_result_ = a.tag () == b.tag ();
    if (!visiting_result_)
      location () << dwarf::tags::name (a.tag ())
		  << " vs "
		  << dwarf::tags::name (b.tag ())
		  << endl;
  }

  inline bool keep_going ()
  {
    result_ = visiting_result_ = false;
    return print_all;
  }

  inline bool notice_match (typename subtracker::reference_match &matched,
			    const die1 &a, const die2 &b, bool matches)
  {
    /* In the main walk, a mismatch would have gone to keep_going.
       But in reference_match, the comparator uses a subtracker.
       It won't have set visiting_result_, but it will return a
       real non-matching result we can catch here.  */
    if (!matches)
      visiting_result_ = false;

    // Record the real result in the cache, not a fake match for -l.
    subtracker::notice_match (matched, a, b, visiting_result_);

    return matches || keep_going ();
  }

  inline bool mismatch (cu1 &it1, const cu1 &end1,
			cu2 &it2, const cu2 &end2)
  {
    if (it1 == end1)		// a lacks some of b's CUs.
      {
	cout << "files differ: "
	     << dec << subr::length (it2, end2)
	     << " extra compilation units "
	     << endl;
	it2 = end2;
      }
    else if (it2 == end2)	// b lacks some of a's CUs.
      {
	cout << "files differ: "
	     << dec << subr::length (it1, end1)
	     << " compilation units missing "
	     << endl;
	it1 = end1;
      }
    else
      // Otherwise the differing CU will have announced itself.
      ++it1, ++it2;
    return keep_going ();
  }

  inline bool mismatch (die1 &it1, const die1 &end1,
			die2 &it2, const die2 &end2)
  {
    if (it1 == end1)		// a_ lacks some of b_'s children.
      {
	location () << dec << subr::length (it2, end2)
		    << " extra children " << endl;
	it2 = end2;
      }
    else if (it2 == end2)	// b_ lacks some of a_'s children.
      {
	location () << dec << subr::length (it1, end1)
		    << " children missing " << endl;
	it1 = end1;
      }
    else
      // Otherwise the differing child will have announced itself.
      ++it1, ++it2;
    return keep_going ();
  }

  inline bool mismatch (attr1 &it1, const attr1 &end1,
			attr2 &it2, const attr2 &end2)
  {
    if (it1 == end1)		// a_ lacks some of b_'s attrs.
      {
	for (location () << " extra attributes:"; it2 != end2; ++it2)
	  cout << " " << to_string (*it2);
	it2 = end2;
      }
    else if (it2 == end2)	// b_ lacks some of a_'s attrs.
      {
	for (location () << " missing attributes:"; it1 != end1; ++it1)
	  cout << " " << to_string (*it1);
	it1 = end1;
      }
    else
      {
	location () << to_string (*it1) << " vs " << to_string (*it2);
	if ((*it1).second.what_space () == dwarf::VS_reference
	    && (*it2).second.what_space () == dwarf::VS_reference)
	  reference_mismatch ((*it1).second.reference (),
			      (*it2).second.reference ());
	++it1;
	++it2;
      }
    cout << endl;
    return keep_going ();
  }

  typedef dwarf_comparator<dwarf1, dwarf2, false, subtracker> subcomparator;
  inline void reference_mismatch (const die1 &ref1, const die2 &ref2)
  {
    subcomparator cmp (*(subtracker *) this);
    if (cmp.equals (ref1, ref2) && !print_all)
      cout << " (XXX refs now equal again!)"
	   << (cmp.equals (*ref1, *ref2) ? "" : " (and not identical!!)");
    else if (cmp.equals (*ref1, *ref2))
      {
	cout << " (identical but contexts mismatch)";
	bad_context_[ref1].insert (ref2);
      }
    else
      {
	_base notracker;
	dwarf_comparator<dwarf1, dwarf2, true> cmp_norefs (notracker);
	if (cmp_norefs.equals (*ref1, *ref2))
	  cout << " (" << ref1->to_string () << " with reference mismatches)";
	else
	  cout << " (" << ref1->to_string ()
	       << " != " << ref2->to_string ()
	       << ")";
	if (reference_mismatches_[ref1].insert (ref2).second)
	  mismatches_pending_ = true;
      }
  }

  struct prefixer
  {
    string &string_;
    inline explicit prefixer (string &p) : string_ (p) {}
    inline ~prefixer ()
    {
      string_.clear ();
    }
  };

  inline void
  for_context_map (const context_map &context,
		   void (talker::*method) (const die1 &, const die2 &))
  {
    for (typename context_map::const_iterator i = context.begin ();
	 i != context.end ();
	 ++i)
      for (typename die2_set::const_iterator j = i->second.begin ();
	   j != i->second.end ();
	   ++j)
	(this->*method) (i->first, *j);
  }

  inline void print_one_reference_mismatch (const die1 &ref1, const die2 &ref2)
  {
    if (mismatches_printed_.insert (identity_pair ((*ref1).identity (),
						   (*ref2).identity ())).second)
      {
	prefixer here (prefix_);
	{
	  ostringstream pfx;
	  pfx << "reference "
	      << hex << (*ref1).offset () << " vs " << (*ref2).offset ()
	      << ": ";
	  prefix_ = pfx.str ();
	}
	dwarf_comparator<dwarf1, dwarf2, false, talker> (*this)
	  .equals (ref1, ref2);
      }
  }

  inline void print_reference_mismatches ()
  {
    // Print the pending mismatches and loop if that adds more fresh ones.
    while (mismatches_pending_)
      {
	mismatches_pending_ = false;
	for_context_map (reference_mismatches_,
			 &talker::print_one_reference_mismatch);
      }
  }

  inline void print_one_bad_context (const die1 &ref1, const die2 &ref2)
  {
    dwarf_comparator<dwarf1, dwarf2, true, talker> cmp (*this);

    prefixer here (prefix_);
    {
      ostringstream pfx;
      pfx << hex << (*ref1).offset () << " vs " << (*ref2).offset ()
	  << " context: ";
      prefix_ = pfx.str ();
    }

    typename subtracker::left_context_type left = left_context (ref1);
    typename subtracker::right_context_type right = right_context (ref2);

    left.pop ();
    right.pop ();
    while (!left.empty ())
      {
	if (right.empty ())
	  {
	    cout << prefix_
		 << (*left.const_top ()).offset () << " vs top-level" << endl;
	    return;
	  }

	// This prints the differences if it finds some.
	visit (*left.const_top (), *right.const_top ());
	if (!visiting_result_)
	  {
	    cout << endl;
	    return;
	  }
	if (!cmp.equals (a_->attributes (), b_->attributes ()))
	  return;

	left.pop ();
	right.pop ();
      }
    if (!right.empty ())
      cout << prefix_
	   << "top-level vs " << (*right.const_top ()).offset () << endl;
  }

  inline void print_bad_context ()
  {
    for_context_map (bad_context_, &talker::print_one_bad_context);
  }
};

template<class dw>
static inline typename dw::compile_units::const_iterator
find_cu (const dw &file, const typename dw::debug_info_entry &entry)
{
  dwarf::debug_info_entry::identity_type id = entry.compile_unit ().identity ();

  for (typename dw::compile_units::const_iterator cu
	 = file.compile_units ().begin ();
       cu != file.compile_units ().end ();
       ++cu)
    {
      if ((*cu).identity () == id)
	return cu;
    }

  throw std::logic_error ("cannot find compile_unit for debug_info_entry!");
}

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
{
  typedef dwarf_ref_tracker<dwarf1, dwarf2> my_tracker;

  inline bool operator () (const dwarf1 &a, const dwarf2 &b)
  {
    return equals (a, b);
  }

  inline bool operator () (const dwarf1 &file1, const dwarf2 &file2,
			   const typename dwarf1::debug_info_entry &a,
			   const typename dwarf2::debug_info_entry &b)
  {
    typename my_tracker::walk in (&this->_m_tracker,
				  find_cu (file1, a), find_cu (file2, b));
    in.jump (a, b);
    return equals (a, b);
  }
};

template<class dwarf1, class dwarf2>
static inline bool
quiet_compare (const dwarf1 &a, const dwarf2 &b)
{
  return quiet_cmp<dwarf1, dwarf2> () (a, b);
}

template<class dwarf1, class dwarf2>
static inline bool
quiet_compare (const dwarf1 &file1, const dwarf2 &file2,
	       const typename dwarf1::debug_info_entry &a,
	       const typename dwarf2::debug_info_entry &b)

{
  return quiet_cmp<dwarf1, dwarf2> () (file1, file2, a, b);
}

// To be noisy, the talker wraps the standard tracker with verbosity hooks.
template<class dwarf1, class dwarf2, bool print_all>
struct noisy_cmp
  : public cmp<dwarf1, dwarf2, talker<dwarf1, dwarf2, print_all> >
{
  typedef talker<dwarf1, dwarf2, print_all> my_tracker;

  template<typename item1, typename item2>
  inline bool compare (const item1 &a, const item2 &b)
  {
    if (equals (a, b) && this->_m_tracker.result_)
      {
	assert (this->_m_tracker.bad_context_.empty ());
	return true;
      }
    this->_m_tracker.print_reference_mismatches ();
    this->_m_tracker.print_bad_context ();
    return false;
  }

  inline bool operator () (const dwarf1 &a, const dwarf2 &b)
  {
    return compare (a, b);
  }

  inline bool operator () (const dwarf1 &file1, const dwarf2 &file2,
			   const typename dwarf1::debug_info_entry &a,
			   const typename dwarf2::debug_info_entry &b)
  {
    typename my_tracker::walk in (&this->_m_tracker,
				  find_cu (file1, a), find_cu (file2, b));
    in.jump (a, b);
    return compare (a, b);
  }
};

template<class dwarf1, class dwarf2>
static inline bool
noisy_compare (const dwarf1 &a, const dwarf2 &b, bool print_all)
{
  return (print_all
	  ? noisy_cmp<dwarf1, dwarf2, true> () (a, b)
	  : noisy_cmp<dwarf1, dwarf2, false> () (a, b));
}

template<class dwarf1, class dwarf2>
static inline bool
noisy_compare (const dwarf1 &file1, const dwarf2 &file2,
	       const typename dwarf1::debug_info_entry &a,
	       const typename dwarf2::debug_info_entry &b,
	       bool print_all)
{
  return (print_all
	  ? noisy_cmp<dwarf1, dwarf2, true> () (file1, file2, a, b)
	  : noisy_cmp<dwarf1, dwarf2, false> () (file1, file2, a, b));
}


#ifdef TEST

# include "c++/dwarf_edit"
# include "c++/dwarf_output"

// Test that one comparison works as expected.
template<class dwarf1, class dwarf2>
static void
test_compare (const dwarf1 &file1, const dwarf2 &file2, bool expect)
{
  if (quiet_cmp<dwarf1, dwarf2> () (file1, file2) != expect)
    {
      if (expect)
	noisy_compare (file1, file2, true);
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
  dwarf_output out2 (in2, c2);

  test_classes (file1, file2, out1, out2, same);

  if (two_tests)
    test_classes (in1, in2, out1, out2, same);
}

static void
do_writer_test (dwarf &file1, dwarf &file2, bool same)
{
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
}

#else

static inline void do_writer_test (dwarf &, dwarf &, bool) {}

#endif	// TEST


static inline const dwarf::debug_info_entry
parse_offset (const char *name, dwarf &file, const char *arg)
{
  char *end = NULL;
  Dwarf_Off offset = strtoull (arg, &end, 16);
  if (end == NULL || *end != '\0')
    {
      fputs (gettext ("Invalid offset parameter (use hex).\n"),
	     stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE,
		 program_invocation_short_name);
      exit (1);
    }
  try
    {
      return dwarf::debug_info_entry (file, offset);
    }
  catch (std::runtime_error x)
    {
      fprintf (stderr, gettext ("%s: offset %#" PRIx64 ": %s\n"),
	       name, offset, x.what ());
      exit (1);
    }
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
  if (unlikely (remaining + 2 != argc)
      && unlikely (remaining + 4 != argc))
    {
      fputs (gettext ("Invalid number of parameters.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE, program_invocation_short_name);
      exit (1);
    }
  const char *const fname1 = argv[remaining++];
  int fd1;
  Dwarf *dw1 = open_file (fname1, &fd1);

  const char *const fname2 = argv[remaining++];
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

      if (remaining < argc)
	{
	  const dwarf::debug_info_entry a = parse_offset (fname1, file1,
							  argv[remaining++]);
	  const dwarf::debug_info_entry b = parse_offset (fname2, file2,
							  argv[remaining++]);

	  bool same = (quiet
		       ? quiet_compare (file1, file2, a, b)
		       : noisy_compare (file1, file2, a, b, verbose));

	  result = !same;
	}
      else
	{
	  bool same = (quiet
		       ? quiet_compare (file1, file2)
		       : noisy_compare (file1, file2, verbose));

	  do_writer_test (file1, file2, same);

	  result = !same;
	}
    }

  dwarf_end (dw1);
  dwarf_end (dw2);

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

    case 'l':
      verbose = true;
      break;

    case 'i':
      missing_ok = true;
      break;

#ifdef TEST
    case 'T':
      ++test_writer;
      break;
#endif

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
