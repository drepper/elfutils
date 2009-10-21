#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include <stdexcept>
#include <string>
#include <iostream>
#include "where.h"
#include "main.hh"

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
T *
toplev_check (dwarflint &lint,
	      __attribute__ ((unused)) T *tag = NULL)
{
  try
    {
      return lint.check<T> ();
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
    toplev_check <T> (lint);
  }
};

#endif//DWARFLINT_CHECKS_HH
