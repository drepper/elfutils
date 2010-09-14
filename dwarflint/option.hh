/* Pedantic checker for DWARF files
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

#ifndef DWARFLINT_OPTION_HH
#define DWARFLINT_OPTION_HH

#include <string>
#include <argp.h>
#include <map>
#include <vector>
#include <cassert>

#include <iostream>

class option_common;

class options
  : private std::map<int, option_common *>
{
  friend class option_common;
  std::vector<argp_option> _m_opts;

  option_common *opt (int key) const;
  static error_t parse_opt (int key, char *arg, argp_state *state);

public:
  static options &registered ();
  option_common const *getopt (int key) const;
  argp build_argp ();
};

class option_common
{
  argp_option _m_opt;

  static int _m_last_opt;
  static int get_short_option (char opt_short);

protected:
  bool _m_seen;

  option_common (char const *description,
		 char const *arg_description,
		 char const *opt_long, char opt_short,
		 int flags = 0);

public: // consumer interface
  bool seen () const { return _m_seen; }

public: // option handler interface
  argp_option const &build_option () const { return _m_opt; }
  virtual error_t parse_opt (char *arg, argp_state *state) = 0;
};

template<class arg_type>
class value_converter;

template<class arg_type>
class xoption
  : public option_common
{
  arg_type _m_arg;

public:
  xoption (char const *description,
	   char const *arg_description,
	   char const *opt_long, char opt_short = 0,
	   int flags = 0)
    : option_common (description, arg_description, opt_long, opt_short, flags)
  {
  }

  arg_type const &value () const
  {
    return _m_arg;
  }

  error_t parse_opt (char *arg, __attribute__ ((unused)) argp_state *state)
  {
    _m_seen = true;
    _m_arg = value_converter<arg_type>::convert (arg);
    return 0;
  }
};

template<>
class xoption<void>
  : public option_common
{
public:
  xoption (char const *description,
	   char const *opt_long, char opt_short = 0, int flags = 0)
    : option_common (description, NULL, opt_long, opt_short, flags)
  {
  }

  error_t parse_opt (char *arg, __attribute__ ((unused)) argp_state *state)
  {
    assert (arg == NULL);
    _m_seen = true;
    return 0;
  }

  // This shouldn't be promoted to option_common, as
  // e.g. xoption<bool> will naturally have a different
  // implementation.
  operator bool () { return seen (); }
};

template<>
struct value_converter<std::string>
{
  static std::string convert (char const *arg)
  {
    if (arg == NULL)
      return "";
    else
      return arg;
  }
};

typedef xoption<void> void_option;
typedef xoption<std::string> string_option;

#endif//DWARFLINT_OPTION_HH
