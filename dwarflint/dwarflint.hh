#ifndef DWARFLINT_HH
#define DWARFLINT_HH

#include <map>
#include <vector>
#include <stdexcept>
#include "../libelf/libelf.h"
#include "checks.ii"

class checkstack
  : public std::vector <checkdescriptor const *>
{
};

struct check_rule
{
  enum action_t
    {
      forbid,
      request,
    };

  std::string name;
  action_t action;

  check_rule (std::string const &a_name, action_t an_action)
    : name (a_name)
    , action (an_action)
  {}
};
class check_rules
  : public std::vector<check_rule>
{
  friend class dwarflint;
  bool should_check (checkstack const &stack) const;
};

class dwarflint
{
  typedef std::map <void const *, class check_base *> check_map;
  check_map _m_checks;
  char const *_m_fname;
  int _m_fd;
  check_rules const &_m_rules;

  static void *const marker;

  // Return a pointer to check, or NULL if the check hasn't been done
  // yet.  Throws check_base::failed if the check was requested
  // earlier but failed, or aborts program via assertion if recursion
  // was detected.
  void *find_check (void const *key);

public:
  struct check_registrar
  {
    struct item
    {
      virtual void run (checkstack &stack, dwarflint &lint) = 0;
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
    void enroll (dwarflint &lint);

    std::vector <item *> _m_items;
  };

  dwarflint (char const *fname, check_rules const &rules);
  ~dwarflint ();
  int fd () { return _m_fd; }
  char const *fname () { return _m_fname; }

  template <class T> T *check (checkstack &stack);

  template <class T>
  T *
  check (checkstack &stack,
	 __attribute__ ((unused)) T *fake)
  {
    return check<T> (stack);
  }

  template <class T> T *toplev_check (checkstack &stack,
				      T *tag = NULL);
};

#endif//DWARFLINT_HH
