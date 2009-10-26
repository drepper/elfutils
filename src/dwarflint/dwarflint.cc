#include "dwarflint.hh"

dwarflint::dwarflint (Elf *a_elf)
  : _m_elf (a_elf)
{
  check_registrar::inst ()->enroll (*this);
}
