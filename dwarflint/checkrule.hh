/*
   Copyright (C) 2010 Red Hat, Inc.
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

#ifndef DWARFLINT_CHECKRULE_HH
#define DWARFLINT_CHECKRULE_HH

#include <vector>
#include <string>
#include "dwarflint_i.hh"

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
