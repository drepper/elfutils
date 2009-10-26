#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include <stdexcept>
#include <string>
#include <iostream>
#include "where.h"
#include "dwarflint.hh"

struct check_base
{
  struct failed
    : public std::runtime_error
  {
    failed (std::string const &msg)
      : std::runtime_error (msg)
    {}
  };

  static failed unscheduled;
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
inline T *
dwarflint::toplev_check (__attribute__ ((unused)) T *tag)
{
  try
    {
      return check<T> ();
    }
  catch (check_base::failed const &f)
    {
      std::cout << f.what () << std::endl;
      return NULL;
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
    lint.toplev_check <T> ();
  }
};

#endif//DWARFLINT_CHECKS_HH
