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

#include "check_registrar.hh"
#include "checkdescriptor.hh"
#include "dwarflint.hh"
#include "main.hh"
#include "wrap.hh"

void
check_registrar_aux::add_deps (std::set<checkdescriptor const *> &to,
				checkdescriptor const *cd)
{
  for (std::set<checkdescriptor const *>::const_iterator it
	 = cd->prereq ().begin (); it != cd->prereq ().end (); ++it)
    include (to, *it);
}

void
check_registrar_aux::include (std::set<checkdescriptor const *> &to,
			       checkdescriptor const *cd)
{
  if (cd->hidden ())
    add_deps (to, cd);
  else
    to.insert (cd);
}

bool
check_registrar_aux::be_verbose ()
{
  // We can hopefully assume that the option doesn't change during
  // execution, so we can simply cache it this was.
  static bool be_verbose = opt_list_checks.value () == "full";
  return be_verbose;
}

void
check_registrar_aux::list_one_check (checkdescriptor const &cd)
{
  const size_t columns = 70;

  if (be_verbose ())
    std::cout << "=== " << cd.name () << " ===";
  else
    std::cout << cd.name ();

  checkgroups const &groups = cd.groups ();
  if (!groups.empty ())
    {
      if (be_verbose ())
	std::cout << std::endl << "groups: ";
      else
	std::cout << ' ';
      std::cout << groups;
    }
  std::cout << std::endl;

  if (be_verbose ())
    {
      prereqs const &prereq = cd.prereq ();
      if (!prereq.empty ())
	std::cout << "prerequisites: " << prereq << std::endl;

      char const *desc = cd.description ();
      if (desc != NULL)
	std::cout << wrap_str (desc, columns).join ();

      options const &opts = cd.opts ();
      if (!opts.empty ())
	{
	  std::cout << "recognized options:" << std::endl;
	  argp a = opts.build_argp ();
	  argp_help (&a, stdout, ARGP_HELP_LONG, NULL);
	}

      std::cout << std::endl;
    }
}
