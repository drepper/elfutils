#include "dwarfstrings.h"
#include "pri.hh"
#include <sstream>

pri::attr::attr (int attr_name)
  : pribase (dwarf_attr_string (attr_name))
{}

std::ostream &
pri::operator << (std::ostream &os, pri::pribase const &obj)
{
  return os << obj.m_a << obj.m_b << obj.m_c;
}

std::ostream &
pri::operator << (std::ostream &os, pri::ref const &obj)
{
  std::stringstream ss;
  ss << std::hex << "DIE " << obj.off;
  return os << ss.str ();
}

std::ostream &
pri::operator << (std::ostream &os, pri::addr const &obj)
{
  std::stringstream ss;
  ss << std::hex << "0x" << obj.off;
  return os << ss.str ();
}

std::ostream &
pri::operator << (std::ostream &os, pri::range const &obj)
{
  return os << "[" << pri::addr (obj.start)
	    << ", " << pri::addr (obj.end) << ")";
}
