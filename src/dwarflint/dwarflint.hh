#ifndef DWARFLINT_HH
#define DWARFLINT_HH

#include <map>
#include <vector>
#include <stdexcept>
#include "../libelf/libelf.h"

class dwarflint
{
  typedef std::map <void const *, class check_base *> check_map;
  check_map _m_checks;
  char const *_m_fname;
  int _m_fd;

public:
  struct check_registrar
  {
    struct item
    {
      virtual void run (dwarflint &lint) = 0;
    };

    static check_registrar *inst ()
    {
      static check_registrar inst;
      return &inst;
    }

    void add (item *i)
    {
      _m_items.push_back (i);
    }

  private:
    friend class dwarflint;
    void enroll (dwarflint &lint)
    {
      for (std::vector <item *>::iterator it = _m_items.begin ();
	   it != _m_items.end (); ++it)
	(*it)->run (lint);
    }

    std::vector <item *> _m_items;
  };

  explicit dwarflint (char const *fname);
  ~dwarflint ();
  int fd () { return _m_fd; }
  char const *fname () { return _m_fname; }

  template <class T> T *check ();

  template <class T>
  T *
  check (__attribute__ ((unused)) T *fake)
  {
    return check<T> ();
  }

  template <class T> T * toplev_check (T *tag = NULL);
};

#endif//DWARFLINT_HH
