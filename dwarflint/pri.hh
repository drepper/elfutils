/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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

#ifndef DWARFLINT_PRI_H
#define DWARFLINT_PRI_H

#include "../libdw/libdw.h"
#include <string>

#define PRI_NOT_ENOUGH ": not enough data for %s.\n"

namespace pri
{
  class pribase
  {
    std::string m_s;

  protected:
    pribase (std::string const &a,
	     std::string const &b = "",
	     std::string const &c = "")
      : m_s (a + b + c)
    {}
    friend std::ostream &operator << (std::ostream &os, pribase const &obj);

  public:
    operator std::string const &() const { return m_s; }
  };
  std::ostream &operator << (std::ostream &os, pribase const &obj);

  struct not_enough
    : public pribase
  {
    not_enough (std::string const &what)
      : pribase ("not enough data for ", what)
    {}
  };

  struct lacks_relocation
    : public pribase
  {
    lacks_relocation (std::string const &what)
      : pribase (what, " seems to lack a relocation")
    {}
  };

  struct form
    : public pribase
  {
    form (int attr_form);
  };

  struct locexpr_opcode
    : public pribase
  {
    locexpr_opcode (int opcode);
  };

  class ref
  {
    Dwarf_Off off;
  public:
    template <class T>
    ref (T const &die)
      : off (die.offset ())
    {}
    friend std::ostream &operator << (std::ostream &os, ref const &obj);
  };
  std::ostream &operator << (std::ostream &os, ref const &obj);

  class hex
  {
    Dwarf_Off value;
    char const *const pre;
  public:
    hex (Dwarf_Off a_value, char const *a_pre = NULL)
      : value (a_value)
      , pre (a_pre)
    {}
    friend std::ostream &operator << (std::ostream &os, hex const &obj);
  };
  std::ostream &operator << (std::ostream &os, hex const &obj);

  struct addr: public hex {
    addr (Dwarf_Off off) : hex (off) {}
  };

  struct DIE: public hex {
    DIE (Dwarf_Off off) : hex (off, "DIE ") {}
  };

  struct CU: public hex {
    CU (Dwarf_Off off) : hex (off, "CU ") {}
  };

  class range
  {
    Dwarf_Off start;
    Dwarf_Off end;
  public:
    range (Dwarf_Off a_start, Dwarf_Off a_end)
      : start (a_start), end (a_end)
    {}
    friend std::ostream &operator << (std::ostream &os, range const &obj);
  };
  std::ostream &operator << (std::ostream &os, range const &obj);
}

#endif//DWARFLINT_PRI_H
