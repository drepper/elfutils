#include "dwarfstrings.h"
#include "pri.hh"
#include <sstream>

std::ostream &
pri::operator << (std::ostream &os, pri::pribase const &obj)
{
  return os << obj.m_s;
}

pri::attr::attr (int attr_name)
  : pribase (dwarf_attr_string (attr_name))
{}

pri::form::form (int attr_form)
  : pribase (dwarf_form_string (attr_form))
{}

pri::tag::tag (int die_tag)
  : pribase (dwarf_tag_string (die_tag))
{}

pri::locexpr_opcode::locexpr_opcode (int opcode)
  : pribase (dwarf_locexpr_opcode_string (opcode))
{}

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
  ss << std::hex << "0x" << obj.value;
  return os << ss.str ();
}

std::ostream &
pri::operator << (std::ostream &os, pri::range const &obj)
{
  return os << "[" << pri::addr (obj.start)
	    << ", " << pri::addr (obj.end) << ")";
}
