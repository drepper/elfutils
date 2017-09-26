/* Pedantic checking of DWARF files
   Copyright (C) 2011 Red Hat, Inc.
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
    checkdescriptors_t ret;
    for (typename _super_t::const_iterator it = begin (); it != end (); ++it)
      ret.push_back ((*it)->descriptor ());
    return ret;
  }
};

#endif /* _CHECK_REGISTRAR_H_ */
