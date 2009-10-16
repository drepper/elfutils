#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include <stdexcept>
#include <string>
#include <iostream>
#include "dwarflint-where.h"
#include "dwarflint-low.h"
#include "dwarflint-main.hh"

struct check_base
{
  struct failed
    : public std::runtime_error
  {
    failed (std::string const &msg)
      : std::runtime_error (msg)
    {}
  };
};

template<class T>
class check
  : public check_base
{
public:
  static void const *key ()
  {
    return reinterpret_cast <void const *> (&key);
  }
};

template <class T>
void
toplev_check (dwarflint &lint)
{
  try
    {
      lint.check<T> ();
    }
  catch (check_base::failed const &f)
    {
      std::cout << f.what () << std::endl;
    }
}

template <class T>
struct reg
  : public dwarflint::check_registrar::item
{
  reg ()
  {
    dwarflint::check_registrar::inst ()->add (this);
  }

  virtual void run (dwarflint &lint)
  {
    toplev_check <T> (lint);
  }
};

struct section_base
  : public check<section_base>
  , public sec
{
  section_base (dwarflint &lint, section_id secid);
};

template<section_id sec_id>
class section
  : public section_base
{
public:
  explicit section (dwarflint &lint)
    : section_base (lint, sec_id)
  {}
};

#endif//DWARFLINT_CHECKS_HH
