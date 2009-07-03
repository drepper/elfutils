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

#include "c++/dwarf_edit"
#include "c++/dwarf_output"

static bool print_offset;

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

  depth = 0;
  if (argc > 1 && sscanf (argv[1], "--depth=%u", &depth) == 1)
    {
      --argc;
      ++argv;
    }
}

typedef tr1::unordered_map< ::Dwarf_Off, int> refs_map;

static void
finish_refs_map (refs_map &refs)
{
  int id = 0;
  for (refs_map::iterator it = refs.begin (); it != refs.end (); ++it)
    it->second = ++id;
}

template<typename file>
static void
prewalk_die (const typename file::debug_info_entry &die, refs_map &refs)
{
  for (typename file::debug_info_entry::children_type::const_iterator i
	 = die.children ().begin (); i != die.children ().end (); ++i)
    prewalk_die<file> (*i, refs);

  for (typename file::debug_info_entry::attributes_type::const_iterator i
	 = die.attributes ().begin (); i != die.attributes ().end (); ++i)
    if ((*i).second.what_space () == dwarf::VS_reference)
      refs[(*i).second.reference ()->identity ()];
}

template<typename file>
static void
print_die (const typename file::debug_info_entry &die,
	   unsigned int indent, unsigned int limit, refs_map &refs)
{
  string prefix (indent, ' ');
  const string tag = dwarf::tags::name (die.tag ());

  cout << prefix << "<" << tag;
  if (print_offset)
    cout << " offset=[" << die.offset () << "]";
  else
    {
      refs_map::const_iterator it = refs.find (die.identity ());
      if (it != refs.end ())
	cout << " ref=\"" << hex << it->second << "\"";
    }

  for (typename file::debug_info_entry::attributes_type::const_iterator i
	 = die.attributes ().begin (); i != die.attributes ().end (); ++i)
    {
      if (!print_offset && (*i).second.what_space () == dwarf::VS_reference)
	cout << " " << dwarf::attributes::name ((*i).first) << "=\"#"
	     << hex << refs[(*i).second.reference ()->identity ()] << "\"";
      else
	cout << " " << to_string (*i);
    }

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

template<typename file>
static void
print_cu (const typename file::compile_unit &cu, const unsigned int limit)
{
  const typename file::debug_info_entry &die = cu;
  // static_cast<const typename file::debug_info_entry &> (cu),

  refs_map refs;

  if (!print_offset)
    {
      prewalk_die<file> (die, refs);
      finish_refs_map (refs);
    }

  print_die<file> (die, 1, limit, refs);
}

template<typename file>
static void
print_file (const file &dw, const unsigned int limit)
{
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
#if 0 // XXX
    case copy_output:
      print_file (dwarf_output (dw), limit);
      break;
#endif
    default:
      abort ();
    }
}
