#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <cassert>

enum optionality
{
  opt_optional = 0,	// may or may not be present
  opt_required,		// bogus if missing
  opt_expected,		// suspicious if missing
};

template <class T>
std::string
to_string (T x)
{
  std::ostringstream o;
  o << x;
  return o.str();
}

struct expected_set
{
  typedef std::map <int, optionality> expectation_map;

private:
  expectation_map m_map;

public:
#define DEF_FILLER(WHAT)						\
  expected_set &WHAT (int attribute)					\
  {									\
    assert (m_map.find (attribute) == m_map.end ());			\
    m_map.insert (std::make_pair (attribute, opt_##WHAT));		\
    return *this;							\
  }									\
  expected_set &WHAT (std::set <int> const &attributes)			\
  {									\
    for (std::set <int>::const_iterator it = attributes.begin ();	\
	 it != attributes.end (); ++it)					\
      WHAT (*it);							\
    return *this;							\
  }

  DEF_FILLER (required)
  DEF_FILLER (expected)
  DEF_FILLER (optional)
#undef DEF_FILLER

  expectation_map const &map () const
  {
    return m_map;
  }
};

class expected_map
{
  typedef std::map <int, expected_set> expected_map_t;

protected:
  expected_map_t m_map;
  expected_map () {}

public:
  expected_set::expectation_map const &map (int tag) const
  {
    expected_map_t::const_iterator it = m_map.find (tag);
    if (it == m_map.end ())
      throw std::runtime_error ("Unknown tag #" + to_string (tag));
    return it->second.map ();
  }
};

struct expected_at_map
  : public expected_map
{
  expected_at_map ();
};
