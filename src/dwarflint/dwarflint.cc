#include "dwarflint.hh"

dwarflint::dwarflint (int a_fd)
  : _m_fd (a_fd)
{
  check_registrar::inst ()->enroll (*this);
}
