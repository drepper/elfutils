/* Pedantic checking of DWARF files.
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

#include <vector>
#include <stdexcept>

// Tree flattening iterator.  It pre-order iterates all CUs in given
// dwarf file.
template <class T>
class all_dies_iterator
  : public std::iterator<std::input_iterator_tag,
			 typename T::debug_info_entry>
{
  typedef typename T::debug_info_entry::children_type::const_iterator die_it_t;
  typedef std::vector <std::pair <die_it_t, die_it_t> > die_it_stack_t;

  typename T::compile_units::const_iterator cu_it, cu_it_end;
  die_it_t die_it, die_it_end;
  die_it_stack_t die_it_stack;
  bool atend;

  void nest ()
  {
    while (die_it->has_children ())
      {
	die_it_stack.push_back (std::make_pair (die_it, die_it_end));
	die_it_end = (*die_it).children ().end ();
	die_it = (*die_it).children ().begin ();
      }
  }

  void start ()
  {
    die_it = die_it_t (*cu_it);
    die_it_end = die_it_t ();
    nest ();
  }

public:
  // An end iterator.
  all_dies_iterator ()
    : atend (true)
  {}

  explicit all_dies_iterator (T const &dw)
    : cu_it (dw.compile_units ().begin ())
    , cu_it_end (dw.compile_units ().end ())
    , atend (false)
  {
    start ();
  }

  bool operator== (all_dies_iterator const &other)
  {
    return (atend && other.atend)
      || (cu_it == other.cu_it
	  && die_it == other.die_it
	  && die_it_stack == other.die_it_stack);
  }

  bool operator!= (all_dies_iterator const &other)
  {
    return !(*this == other);
  }

  all_dies_iterator operator++ () // prefix
  {
    if (!atend)
      {
	if (++die_it == die_it_end)
	  {
	    if (die_it_stack.size () > 0)
	      {
		die_it = die_it_stack.back ().first;
		die_it_end = die_it_stack.back ().second;
		die_it_stack.pop_back ();
	      }
	    else if (++cu_it == cu_it_end)
	      atend = true;
	    else
	      start ();
	  }
	else
	  nest ();
      }
    return *this;
  }

  all_dies_iterator operator++ (int) // postfix
  {
    all_dies_iterator prev = *this;
    ++*this;
    return prev;
  }

  typename T::debug_info_entry const &operator* () const
  {
    if (/*unlikely*/ (atend))
      throw std::runtime_error ("dereferencing end iterator");
    return *die_it;
  }

  typename T::debug_info_entry const *operator-> () const
  {
    return &**this;
  }
};
