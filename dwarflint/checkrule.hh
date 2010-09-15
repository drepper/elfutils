/*
   Copyright (C) 2010 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECKRULE_HH
#define DWARFLINT_CHECKRULE_HH

#include <vector>
#include <string>
#include "dwarflint.ii"

struct checkrule
{
  enum action_t
    {
      forbid,
      request,
    };

private:
  std::string _m_name;
  action_t _m_action;
  mutable bool _m_used;

public:
  checkrule (std::string const &name, action_t action);

  std::string const &name () const { return _m_name; }
  action_t action () const { return _m_action; }
  bool used () const { return _m_used; }
  void mark_used () const { _m_used = true; }
};

// These are like normal rules, but they are initially marked as used
// so as not to be warned about.
struct checkrule_internal
  : public checkrule
{
  checkrule_internal (std::string const &name, action_t action);
};

class checkrules
  : public std::vector<checkrule>
{
public:
  bool should_check (checkstack const &stack) const;
};

#endif//DWARFLINT_CHECKRULE_HH
