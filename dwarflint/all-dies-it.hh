/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
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

  typename T::compile_units_type::const_iterator _m_cu_it, _m_cu_it_end;
  die_it_t _m_die_it, _m_die_it_end;
  die_it_stack_t _m_die_it_stack;
  bool _m_atend;

  void start ()
  {
    if (_m_cu_it == _m_cu_it_end)
      _m_atend = true;
    else
      {
	_m_die_it = die_it_t (*_m_cu_it);
	_m_die_it_end = die_it_t ();
	++_m_cu_it;
	assert (_m_die_it != _m_die_it_end);
      }
  }

public:
  // An end iterator.
  all_dies_iterator ()
    : _m_atend (true)
  {}

  explicit all_dies_iterator (T const &dw)
    : _m_cu_it (dw.compile_units ().begin ())
    , _m_cu_it_end (dw.compile_units ().end ())
    , _m_atend (_m_cu_it == _m_cu_it_end)
  {
    if (!_m_atend)
      start ();
  }

  bool operator== (all_dies_iterator const &other)
  {
    return (_m_atend && other._m_atend)
      || (_m_cu_it == other._m_cu_it
	  && _m_die_it == other._m_die_it
	  && _m_die_it_stack == other._m_die_it_stack);
  }

  bool operator!= (all_dies_iterator const &other)
  {
    return !(*this == other);
  }

  all_dies_iterator operator++ () // prefix
  {
    if (!_m_atend)
      {
	if (_m_die_it->has_children ()
	    && _m_die_it->children ().begin () != _m_die_it->children ().end ())
	  {
	    _m_die_it_stack.push_back (std::make_pair (_m_die_it, _m_die_it_end));
	    _m_die_it_end = _m_die_it->children ().end ();
	    _m_die_it = _m_die_it->children ().begin ();
	  }
	else
	  while (++_m_die_it == _m_die_it_end)

	    {
	      if (_m_die_it_stack.size () == 0)
		{
		  start ();
		  break;
		}
	      _m_die_it = _m_die_it_stack.back ().first;
	      _m_die_it_end = _m_die_it_stack.back ().second;
	      _m_die_it_stack.pop_back ();
	    }
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
    if (unlikely (_m_atend))
      throw std::runtime_error ("dereferencing end iterator");
    return *_m_die_it;
  }

  std::vector<typename T::debug_info_entry> stack () const
  {
    std::vector<typename T::debug_info_entry> ret;
    for (auto it = _m_die_it_stack.begin ();
	 it != _m_die_it_stack.end (); ++it)
      ret.push_back (*it->first);
    ret.push_back (*_m_die_it);
    return ret;
  }

  typename T::compile_unit cu () const
  {
    return *_m_cu_it;
  }

  typename T::debug_info_entry const *operator-> () const
  {
    return &**this;
  }
};
