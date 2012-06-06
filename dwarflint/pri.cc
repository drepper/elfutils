/* Pedantic checking of DWARF files
   Copyright (C) 2008,2009,2010,2011 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sstream>

#include "../src/dwarfstrings.h"
#include "../libdw/c++/dwarf"

#include "pri.hh"

std::ostream &
pri::operator << (std::ostream &os, pri::pribase const &obj)
{
  return os << obj.m_s;
}

std::ostream &
pri::operator << (std::ostream &os, pri::ref const &obj)
{
  std::stringstream ss;
  ss << std::hex << "DIE " << obj.off;
  return os << ss.str ();
}

std::ostream &
pri::operator << (std::ostream &os, pri::hex const &obj)
{
  std::stringstream ss;
  if (obj.pre)
    ss << obj.pre << " ";
  ss << std::hex << "0x" << obj.value;
  return os << ss.str ();
}

std::ostream &
pri::operator << (std::ostream &os, pri::range const &obj)
{
  return os << "[" << pri::addr (obj.start)
	    << ", " << pri::addr (obj.end) << ")";
}

std::string
pri::attr_name (int name)
{
  assert (name != -1);
  return elfutils::dwarf::attributes::name (name);
}
