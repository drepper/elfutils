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

#include "messages.hh"
#include "highlevel_check.hh"
#include "check_die_tree.hh"

using namespace elfutils;

namespace
{
  reg<check_die_tree> reg;
}

class die_check_context
  : protected std::vector<die_check *>
{
  typedef std::vector<die_check *> _super_t;

public:
  die_check_context (checkdescriptor const *cd, dwarflint &lint,
		     die_check_registrar const &registrar)
  {
    // For per-DIE runs, we are only interested in limited context:
    // the main iteration check, and the per-DIE check.  This should
    // be enough to decide whether to run the per-DIE check or not.
    // We cannot use the original stack as a criterion, because the
    // original check that tricked us into running is here, and the
    // logic in should_check would then assume that we need to run
    // everything.
    checkstack stack;
    stack.push_back (cd);

    for (die_check_registrar::const_iterator it = registrar.begin ();
	 it != registrar.end (); ++it)
      {
	stack.push_back ((*it)->descriptor ());
	popper p (stack);
	if (lint.rules ().should_check (stack))
	  push_back ((*it)->create (stack, lint));
      }
  }

  void
  die (all_dies_iterator<dwarf> const &a_d_it)
  {
    for (iterator it = begin (); it != end (); ++it)
      (*it)->die (a_d_it);
  }

  ~die_check_context ()
  {
    for (iterator it = begin (); it != end (); ++it)
      delete *it;
  }
};

check_die_tree::check_die_tree (checkstack &stack, dwarflint &lint)
  : highlevel_check<check_die_tree> (stack, lint)
{
  //std::cout << "check_die_tree" << std::endl;
  die_check_context ctx (descriptor (), lint, *dwarflint::die_registrar ());

  for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
       it != all_dies_iterator<dwarf> (); ++it)
    ctx.die (it);
}
