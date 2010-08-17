#ifndef DWARFLINT_PRI_H
#define DWARFLINT_PRI_H

#include <libdw.h>
#include <string>

namespace pri
{
  class pribase
  {
    std::string m_s;

  protected:
    pribase (std::string const &a,
	     std::string const &b = "",
	     std::string const &c = "")
      : m_s (a + b + c)
    {}
    friend std::ostream &operator << (std::ostream &os, pribase const &obj);

  public:
    operator std::string const &() const { return m_s; }
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

  struct locexpr_opcode
    : public pribase
  {
    locexpr_opcode (int opcode);
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
    char const *const pre;
  public:
    hex (Dwarf_Off a_value, char const *a_pre = NULL)
      : value (a_value)
      , pre (a_pre)
    {}
    friend std::ostream &operator << (std::ostream &os, hex const &obj);
  };
  std::ostream &operator << (std::ostream &os, hex const &obj);

  struct addr: public hex {
    addr (Dwarf_Off off) : hex (off) {}
  };

  struct DIE: public hex {
    DIE (Dwarf_Off off) : hex (off, "DIE ") {}
  };

  struct CU: public hex {
    CU (Dwarf_Off off) : hex (off, "CU ") {}
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
