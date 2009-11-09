#ifndef DWARFLINT_PRI_H
#define DWARFLINT_PRI_H

#include <libdw.h>
#include <string>

namespace pri
{
  class pribase
  {
    std::string const &m_a;
    std::string const &m_b;
    std::string const &m_c;

  protected:
    pribase (std::string const &a,
	     std::string const &b = "",
	     std::string const &c = "")
      : m_a (a), m_b (b), m_c (c)
    {}
    friend std::ostream &operator << (std::ostream &os, pribase const &obj);
  };
  std::ostream &operator << (std::ostream &os, pribase const &obj);

  struct not_enough
    : public pribase
  {
    not_enough (std::string const &what)
      : pribase ("not enough data for ", what)
    {}
  };

  struct lacks_relocation
    : public pribase
  {
    lacks_relocation (std::string const &what)
      : pribase (what, " seems to lack a relocation")
    {}
  };

  struct attr
    : public pribase
  {
    attr (int attr_name);
  };

  struct form
    : public pribase
  {
    form (int attr_form);
  };

  struct tag
    : public pribase
  {
    tag (int tag);
  };

  class ref
  {
    Dwarf_Off off;
  public:
    template <class T>
    ref (T const &die)
      : off (die.offset ())
    {}
    friend std::ostream &operator << (std::ostream &os, ref const &obj);
  };
  std::ostream &operator << (std::ostream &os, ref const &obj);

  class hex
  {
    Dwarf_Off value;
  public:
    hex (Dwarf_Off a_value)
      : value (a_value)
    {}
    friend std::ostream &operator << (std::ostream &os, hex const &obj);
  };
  std::ostream &operator << (std::ostream &os, hex const &obj);

  struct addr : public hex {
    addr (Dwarf_Off off) : hex (off) {}
  };

  class range
  {
    Dwarf_Off start;
    Dwarf_Off end;
  public:
    range (Dwarf_Off a_start, Dwarf_Off a_end)
      : start (a_start), end (a_end)
    {}
    friend std::ostream &operator << (std::ostream &os, range const &obj);
  };
  std::ostream &operator << (std::ostream &os, range const &obj);
}

#endif//DWARFLINT_PRI_H
