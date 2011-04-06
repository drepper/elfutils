/* Pedantic checking of DWARF files
   Copyright (C) 2011 Red Hat, Inc.
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

#ifndef _CHECK_REGISTRAR_H_
#define _CHECK_REGISTRAR_H_

#include "checkdescriptor_i.hh"

#include <vector>
#include <set>
#include <iostream>

namespace check_registrar_aux
{
  bool be_verbose ();
  void list_one_check (checkdescriptor const &cd);

  void include (std::set<checkdescriptor const *> &to,
		checkdescriptor const *cd);
  void add_deps (std::set<checkdescriptor const *> &to,
		 checkdescriptor const *cd);
}

template <class Item>
class check_registrar_T
  : protected std::vector<Item *>
{
  typedef std::vector<Item *> _super_t;
public:

  using _super_t::push_back;
  using _super_t::const_iterator;
  using _super_t::begin;
  using _super_t::end;

  typedef std::vector<checkdescriptor const *> checkdescriptors_t;

  checkdescriptors_t
  get_descriptors () const
  {
    std::set<checkdescriptor const *> descriptors;
    for (typename _super_t::const_iterator it = begin (); it != end (); ++it)
      check_registrar_aux::include (descriptors, (*it)->descriptor ());
    return checkdescriptors_t (descriptors.begin (), descriptors.end ());
  }
};

#endif /* _CHECK_REGISTRAR_H_ */
