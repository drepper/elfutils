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

#ifndef _CHECK_DIE_TREE_H_
#define _CHECK_DIE_TREE_H_

#include "all-dies-it.hh"
#include "highlevel_check.hh"
#include "check_die_tree_i.hh"

#include <c++/dwarf>

struct die_check_item
{
  virtual checkdescriptor const *descriptor () const = 0;
  virtual ~die_check_item () {}
  virtual die_check *create (highlevel_check_i *check,
			     checkstack &stack, dwarflint &lint) = 0;
};

/// Top-level check that iterates over all DIEs in a file and
/// dispatches per-DIE checks on each one.  Per-DIE checks are written
/// as subclasses of die_check (see below) and registered using
/// reg_die_check (see further below).
class check_die_tree
  : public highlevel_check<check_die_tree>
{
public:
  static void register_check (die_check_item *check);

  static checkdescriptor const *descriptor ()
  {
    static checkdescriptor cd
      (checkdescriptor::create ("check_die_tree")
       .inherit<highlevel_check<check_die_tree> > ()
       .hidden ()
       .description ("A pass over the DIE tree that dispatches to various per-DIE checks.\n"));
    return &cd;
  }

  check_die_tree (checkstack &stack, dwarflint &lint);
};

class die_check
{
public:
  virtual ~die_check () {}
  virtual void die (all_dies_iterator<elfutils::dwarf> const &it) = 0;
};

template <class T>
struct reg_die_check
  : public die_check_item
{
  reg_die_check ()
  {
    check_die_tree::register_check (this);
  }

  virtual die_check *create (highlevel_check_i *check,
			     checkstack &stack, dwarflint &lint)
  {
    return new T (check, stack, lint);
  }

  virtual checkdescriptor const *descriptor () const
  {
    return T::descriptor ();
  }

private:
  /// The top-level scheduler needs to see per-DIE checks as real
  /// checks, which they are not.  So the per-DIE registrar creates
  /// this check stub that's here only to trick the check_die_tree to
  /// run.  check_die_tree then does the per-DIE check scheduling
  /// itself, down in die_check_context.
  class check_stub
    : public highlevel_check<check_stub>
  {
    check_die_tree *_m_die_tree_check;
  public:
    static checkdescriptor const *descriptor ()
    {
      static checkdescriptor cd
	(checkdescriptor::create (*T::descriptor ())
	 .prereq<typeof (*_m_die_tree_check)> ()
	 .inherit<highlevel_check<check_stub> > ());
      return &cd;
    }

    check_stub (checkstack &stack, dwarflint &lint)
      : highlevel_check<check_stub> (stack, lint)
      , _m_die_tree_check (lint.check (stack, _m_die_tree_check))
    {}
  };

  ::reg<check_stub> _m_reg_stub;
};

#endif /* _CHECK_DIE_TREE_H_ */
