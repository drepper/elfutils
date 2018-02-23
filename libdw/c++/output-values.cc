/* elfutils::dwarf_output attribute value interfaces.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "dwarf_output"
#include "data-values.hh"

#include <typeinfo>

using namespace elfutils;

// Explicit instantiations.
template class dwarf_data::value<dwarf_output, false>;
template class dwarf_data::attr_value<dwarf_output, dwarf_output::value>;
template class dwarf_data::attributes_type<dwarf_output, dwarf_output::value>;
template class dwarf_data::compile_unit<dwarf_output>;
template class dwarf_data::compile_units_type<dwarf_output>;

template class dwarf_output::copier<dwarf>;
template class dwarf_output::copier<dwarf_edit>;

template<>
std::string
elfutils::to_string<dwarf_output::attribute> (const dwarf_output::attribute &attr)
{
  return attribute_string (attr);
}

namespace elfutils
{
  template<>
  std::string to_string (const dwarf_output::debug_info_entry &die)
  {
    return die_string (die);
  }
};

const dwarf_output::value::value_flag dwarf_output_collector::flag_true (1);
const dwarf_output::value::value_flag dwarf_output_collector::flag_false (0);
