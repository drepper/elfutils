/* Dwarflint check scheduler.
   Copyright (C) 2008,2009 Red Hat, Inc.
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
#include "messages.h"
#include "checks.hh"

#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <sstream>

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

dwarflint::dwarflint (char const *a_fname)
  : _m_fname (a_fname)
  , _m_fd (get_fd (_m_fname))
{
  check_registrar::inst ()->enroll (*this);
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
