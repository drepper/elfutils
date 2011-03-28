/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009,2010 Red Hat, Inc.
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

#include "dwarflint.hh"
#include "messages.hh"
#include "checks.hh"
#include "check_registrar.hh"

#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <sstream>

std::ostream &
operator << (std::ostream &o, checkstack const &stack)
{
  o << "{";
  for (checkstack::const_iterator it = stack.begin ();
       it != stack.end (); ++it)
    {
      if (it != stack.begin ())
	o << ',';
      o << (*it)->name ();
    }
  o << "}";
  return o;
}

namespace
{
  int
  get_fd (char const *fname)
  {
    /* Open the file.  */
    int fd = open (fname, O_RDONLY);
    if (fd == -1)
      {
	std::stringstream ss;
	ss << "Cannot open input file: " << strerror (errno) << ".";
	throw std::runtime_error (ss.str ());
      }

    return fd;
  }
}

void
main_check_registrar::run (dwarflint &lint)
{
  for (const_iterator it = begin (); it != end (); ++it)
    {
      checkstack stack;
      (*it)->run (stack, lint);
    }
}

dwarflint::dwarflint (char const *a_fname, checkrules const &a_rules)
  : _m_fname (a_fname)
  , _m_fd (get_fd (_m_fname))
  , _m_rules (a_rules)
{
  main_registrar ()->run (*this);
}

dwarflint::~dwarflint ()
{
  if (close (_m_fd) < 0)
    // Not that we can do anything about it...
    wr_error () << "Couldn't close the file " << _m_fname << ": "
		<< strerror (errno) << "." << std::endl;
  for (check_map::const_iterator it = _m_checks.begin ();
       it != _m_checks.end (); ++it)
    delete it->second;
}

void *const dwarflint::marker = (void *)-1;

void *
dwarflint::find_check (void const *key)
{
  check_map::const_iterator it = _m_checks.find (key);

  if (it != _m_checks.end ())
    {
      void *c = it->second;

      // We already tried to do the check, but failed.
      if (c == NULL)
	throw check_base::failed ();
      else
	// Recursive dependency!
	assert (c != marker);

      return c;
    }

  return NULL;
}

main_check_registrar *
dwarflint::main_registrar ()
{
  static main_check_registrar inst;
  return &inst;
}

die_check_registrar *
dwarflint::die_registrar ()
{
  static die_check_registrar inst;
  return &inst;
}

namespace
{
  template <class T>
  void
  list_part_checks (T const &descriptors)
  {
    for (typename T::const_iterator it = descriptors.begin ();
	 it != descriptors.end (); ++it)
      check_registrar_aux::list_one_check (**it);
  }
}

void
dwarflint::list_checks ()
{
  list_part_checks (dwarflint::main_registrar ()->get_descriptors ());

  if (!check_registrar_aux::be_verbose ())
    std::cout
      << "Use --list-checks=full to get more detailed description."
      << std::endl;
}

static global_opt<void_option> show_progress
  ("Print out checks as they are performed, their context and result.",
   "show-progress");

namespace
{
  struct reporter
  {
    checkstack const &stack;
    checkdescriptor const &cd;

    reporter (checkstack const &s, checkdescriptor const &a_cd);
    void operator () (char const *what, bool ext = false);
  };

  reporter::reporter (checkstack const &s, checkdescriptor const &a_cd)
    : stack (s)
    , cd (a_cd)
  {
    (*this) ("...", true);
  }

  void
  reporter::operator () (char const *what, bool ext)
  {
    if (!show_progress)
      return;

    if (false)
      for (size_t i = 0; i < stack.size (); ++i)
	std::cout << ' ';

    std::cout << cd.name () << ' ' << what;
    if (ext)
      std::cout << ' ' << cd.groups () << ' ' << stack;
    std::cout << std::endl;
  }
}

void *
dwarflint::dispatch_check (checkstack &stack,
			   checkdescriptor const &cd,
			   void const *key,
			   check_base *(* create) (checkstack &, dwarflint &))
{
  // Put a marker there indicating that we are trying to satisfy
  // that dependency.
  bool inserted
    = _m_checks.insert (std::make_pair (key, (check_base *)marker)).second;
  assert (inserted || !"duplicate key");

#define FAIL					\
  /* Put the anchor in the table.  */		\
    _m_checks[key] = NULL;			\
    report ("FAIL")

  reporter report (stack, cd);
  try
    {
      stack.push_back (&cd);
      popper p (stack);

      if (!_m_rules.should_check (stack))
	throw check_base::unscheduled ();

      // Now do the check.
      check_base *c = create (stack, *this);

      // On success, put the actual check object there instead of the
      // marker.
      _m_checks[key] = c;
      report ("done");
      return c;
    }
  catch (check_base::unscheduled &e)
    {
      report ("skipped");
      _m_checks.erase (key);
      throw;
    }
  catch (check_base::failed &e)
    {
      // We can assume that the check emitted error message.
      FAIL;
      throw;
    }
  catch (std::exception &e)
    {
      wr_error () << "A check failed: " << (cd.name () ?: "(nil)") << ": "
		  << e.what () << std::endl;
      FAIL;
      throw check_base::failed ();
    }
  catch (...)
    {
      wr_error () << "A check failed: " << (cd.name () ?: "(nil)") << "."
		  << std::endl;
      FAIL;
      throw check_base::failed ();
    }

#undef FAIL
}
