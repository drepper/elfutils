/* Pseudo-XMLish printing for elfutils::dwarf* tests.
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

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <libintl.h>
#include <ostream>
#include <iomanip>
#include <tr1/unordered_map>
#include <functional>
#include <algorithm>

#include "c++/dwarf_edit"
#include "c++/dwarf_output"

static bool print_offset;
static bool sort_attrs;
static bool elide_refs;
static bool dump_refs;
static bool no_print;

static enum { copy_none, copy_edit, copy_output } make_copy;

static void
print_die_main (int &argc, char **&argv, unsigned int &depth)
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  cout << hex << setiosflags (ios::showbase);

  if (argc > 1 && !strcmp (argv[1], "--offsets"))
    {
      print_offset = true;
      --argc;
      ++argv;
    }

  if (argc > 1 && !strcmp (argv[1], "--norefs"))
    {
      elide_refs = true;
      --argc;
      ++argv;
    }

  if (argc > 1 && !strcmp (argv[1], "--dump-refs"))
    {
      dump_refs = true;
      --argc;
      ++argv;
    }

  if (argc > 1 && !strcmp (argv[1], "--sort-attrs"))
    {
      sort_attrs = true;
      --argc;
      ++argv;
    }

  if (argc > 1 && !strcmp (argv[1], "--edit"))
    {
      make_copy = copy_edit;
      --argc;
      ++argv;
    }
  else if (argc > 1 && !strcmp (argv[1], "--output"))
    {
      make_copy = copy_output;
      --argc;
      ++argv;
    }

  if (argc > 1 && !strcmp (argv[1], "--silent"))
    {
      no_print = true;
      --argc;
      ++argv;
    }

  depth = 0;
  if (argc > 1 && sscanf (argv[1], "--depth=%u", &depth) == 1)
    {
      --argc;
      ++argv;
    }
}

static int next_ref = 1;
typedef tr1::unordered_map<dwarf::debug_info_entry::identity_type,
			   int> refs_map;

template<typename attrs_type,
	 void (*act) (const typename attrs_type::value_type &, refs_map &)
	 >
class attr_walker
{
private:
  refs_map &refs;
  inline attr_walker (refs_map &r) : refs (r) {}

  typedef typename attrs_type::const_iterator iterator;
  typedef typename iterator::value_type attr_type;

public:
  inline void operator () (const pair<int, iterator> &p) const
  {
    (*act) (*p.second, refs);
  }

  static inline void walk (const attrs_type &attrs, refs_map &r)
  {
    if (attrs_type::ordered () || !sort_attrs)
      for (iterator i = attrs.begin (); i != attrs.end (); ++i)
	(*act) (*i, r);
    else
      {
	map<int, iterator> sorted;
	for (iterator i = attrs.begin (); i != attrs.end (); ++i)
	  sorted[(*i).first] = i;
	for_each (sorted.begin (), sorted.end (),
		  attr_walker<attrs_type, act> (r));
      }
  }
};

template<typename attrs_type>
void
print_attr (const typename attrs_type::value_type &attr, refs_map &refs)
{
  if (!print_offset && attr.second.what_space () == dwarf::VS_reference)
    {
      if (elide_refs)
	cout << " " << dwarf::attributes::name (attr.first) << "=\"ref\"";
      else
	cout << " " << dwarf::attributes::name (attr.first) << "=\"#ref"
	     << dec << refs[attr.second.reference ()->identity ()] << "\"";
    }
  else
    cout << " " << to_string (attr);
}

template<typename attrs_type>
static void
print_attrs (const attrs_type &attrs, refs_map &refs)
{
  attr_walker<attrs_type, print_attr<attrs_type> >::walk (attrs, refs);
}

template<typename attrs_type>
void
prewalk_attr (const typename attrs_type::value_type &attr, refs_map &refs)
{
  if (attr.second.what_space () == dwarf::VS_reference
      && refs.insert (make_pair (attr.second.reference ()->identity (),
				 next_ref)).second)
    ++next_ref;
}

template<typename attrs_type>
static void
prewalk_attrs (const attrs_type &attrs, refs_map &refs)
{
  attr_walker<attrs_type, prewalk_attr<attrs_type> >::walk (attrs, refs);
}

template<typename file>
static void
prewalk_die (const typename file::debug_info_entry &die, refs_map &refs)
{
  for (typename file::debug_info_entry::children_type::const_iterator i
	 = die.children ().begin (); i != die.children ().end (); ++i)
    prewalk_die<file> (*i, refs);

  prewalk_attrs (die.attributes (), refs);
}

static int nth;
static std::map<int, int> nth_ref;

template<typename file>
static void
print_die (const typename file::debug_info_entry &die,
	   unsigned int indent, unsigned int limit, refs_map &refs)
{
  string prefix (indent, ' ');
  const string tag = dwarf::tags::name (die.tag ());

  ++nth;
  if (dump_refs)
    cout << dec << nth << ": ";

  cout << prefix << "<" << tag;
  if (print_offset)
    cout << " offset=[" << hex << die.offset () << "]";
  else if (!elide_refs)
    {
      refs_map::const_iterator it = refs.find (die.identity ());
      if (it != refs.end ())
	{
	  cout << " ref=\"ref" << dec << it->second << "\"";
	  nth_ref[nth] = it->second;
	}
    }

  print_attrs (die.attributes (), refs);

  if (die.has_children ())
    {
      if (limit != 0 && indent >= limit)
	{
	  cout << ">...\n";
	  return;
	}

      cout << ">\n";

      for (typename file::debug_info_entry::children_type::const_iterator i
	     = die.children ().begin (); i != die.children ().end (); ++i)
	print_die<file> (*i, indent + 1, limit, refs);

      cout << prefix << "</" << tag << ">\n";
    }
  else
    cout << "/>\n";
}

static inline void
dump_nth (pair<int, int> p)
{
  cout << dec << p.first << ": ref" << p.second << "\n";
}

template<typename file>
static void
print_cu (const typename file::compile_unit &cu, const unsigned int limit)
{
  const typename file::debug_info_entry &die = cu;
  // static_cast<const typename file::debug_info_entry &> (cu),

  refs_map refs;

  if (!print_offset && !elide_refs)
    prewalk_die<file> (die, refs);

  print_die<file> (die, 1, limit, refs);

  if (dump_refs)
    for_each (nth_ref.begin (), nth_ref.end (), dump_nth);
}

template<typename file>
static void
print_file (const file &dw, const unsigned int limit)
{
  if (no_print)
    return;

  for (typename file::compile_units::const_iterator i
	 = dw.compile_units ().begin (); i != dw.compile_units ().end (); ++i)
    print_cu<file> (*i, limit);
}

template<typename file>
static void
print_file (const char *name, const file &dw, const unsigned int limit)
{
  cout << name << ":\n";

  switch (make_copy)
    {
    case copy_none:
      print_file (dw, limit);
      break;
    case copy_edit:
      print_file (dwarf_edit (dw), limit);
      break;
    case copy_output:
      {
	dwarf_output_collector c; // We'll just throw it away.
	print_file (dwarf_output (dw, c), limit);
      }
      break;
    default:
      abort ();
    }
}
