#ifndef DWARFLINT_CHECKS_HH
#define DWARFLINT_CHECKS_HH

#include <string>
#include "where.h"
#include "dwarflint.hh"

struct check_base
{
  struct failed {};
  struct unscheduled: public failed {};
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
dwarflint::check ()
{
  void const *key = T::key ();
  check_map::iterator it = _m_checks.find (key);

  T *c;
  if (it != _m_checks.end ())
    {
      c = static_cast <T *> (it->second);

      // We already tried to do the check, but failed.
      if (c == NULL)
	throw check_base::failed ();
    }
  else
    {
      // Put a marker there saying that we tried to do the check, but
      // it failed.
      if (!_m_checks.insert (std::make_pair (key, (T *)0)).second)
	throw std::runtime_error ("duplicate key");

      // Now do the check.
      c = new T (*this);

      // On success, put the actual check object there instead of the
      // marker.
      _m_checks[key] = c;
    }
  return c;
}

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
