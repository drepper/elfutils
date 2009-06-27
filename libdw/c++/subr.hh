/* Private helper classes for elfutils -*- C++ -*- interfaces.

 */

#ifndef _ELFUTILS_SUBR_HH
#define _ELFUTILS_SUBR_HH	1

#include <iterator>
#include <functional>
#include <cstring>
#include <iostream>
#include <sstream>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <vector>
#include <algorithm>
#include <utility>

namespace elfutils
{
  namespace subr
  {
    template<typename T>
    struct hash : public T::hasher {};

    template <typename T>
    inline void hash_combine (size_t &seed, const T &v)
    {
      seed ^= hash<T> () (v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <typename T1, typename T2>
    inline void hash_combine (size_t &seed, const std::pair<T1,T2> &v)
    {
      hash_combine (seed, v.first);
      hash_combine (seed, v.second);
    }

    template<typename T>
    struct integer_hash : public std::unary_function<T, size_t>
    {
      inline size_t operator () (const T &x) const
      {
	return x;
      }
    };
    template<>
    struct hash<int> : public integer_hash<int> {};
    template<>
    struct hash<uint64_t> : public integer_hash<uint64_t> {};

    template<typename T>
    struct container_hasher : public std::unary_function<T, size_t>
    {
      struct hasher
      {
	size_t _m_hash;
	inline hasher () : _m_hash (0) {}
	inline void operator () (const typename T::value_type &x)
	{
	  subr::hash_combine (_m_hash, subr::hash<typename T::value_type> (x));
	}
      };

      inline size_t operator () (const T &x) const
      {
	hasher h;
	std::for_each (x.begin (), x.end (), h);
	return h._m_hash;
      }
    };

    template<>
    struct hash<std::string>
    {
    private:
      struct hasher : public container_hasher<std::string>::hasher
      {
	inline void operator () (std::string::value_type c)
	{
	  _m_hash = 5 * _m_hash + c;
	}
      };
    public:
      inline size_t operator () (const std::string &x) const
      {
	hasher h;
	std::for_each (x.begin (), x.end (), h);
	return h._m_hash;
      }
    };

    template<class T>
    struct hashed_hasher
      : public std::unary_function<T, size_t>
    {
      size_t operator () (const T &v)
      {
	return v._m_hash;
      }
    };

    template<typename string>
    struct name_equal : public std::binary_function<const char *, string, bool>
    {
      template<typename mystring>
      inline bool operator () (const mystring &me, const string &you)
      {
	return you == me;
      }
    };

    // Explicit specialization.
    template<>
    struct name_equal<const char *>
      : public std::binary_function<const char *, const char *, bool>
    {
      bool operator () (const char *me, const char *you)
      {
	return !strcmp (me, you);
      }
      template<typename mystring>
      inline bool operator () (const mystring &me, const char *you)
      {
	return me == you;
      }
    };

    static inline std::string hex_string (int code)
    {
      std::ostringstream os;
      os.setf(std::ios::hex, std::ios::basefield);
      os << code;
      return os.str ();
    }

    template<typename prefix_type, const char *lookup_known (int)>
    struct known
    {
      // The names in the table are the identifiers, with prefix.
      static inline std::string identifier (int code)
      {
	const char *known = lookup_known (code);
	return known == NULL ? hex_string (code) : std::string (known);
      }

      // For the pretty name, skip over the prefix.
      static inline std::string name (int code)
      {
	const char *known = lookup_known (code);
	return (known == NULL ? hex_string (code)
		: std::string (&known[sizeof (prefix_type) - 1]));
      }
    };

    template<typename t1, typename t2>
    struct equal_to : public std::binary_function<t1, t2, bool>
    {
      inline bool operator () (const t1 &a, const t2 &b)
      {
	return a == b;
      }
    };

    template<typename t1, typename t2, typename pred_type>
    class deref
      : public std::binary_function<typename t1::const_iterator,
				    typename t2::const_iterator,
				    bool>
    {
    private:
      pred_type _m_pred;

    public:
      inline deref ()
	: _m_pred ()
      {}

      inline deref (const pred_type &pred)
	: _m_pred (pred)
      {}

      inline bool operator () (const typename t1::const_iterator &a,
			       const typename t2::const_iterator &b)
      {
	return _m_pred (*a, *b);
      }
    };

    template<typename t1, typename t2>
    struct deref_equal_to
      : public deref<t1, t2,
		     equal_to<typename t1::value_type, typename t2::value_type>
		     >
    {};

    template<typename iter1, typename iter2, typename pred_type>
    inline bool container_equal (iter1 &first1, const iter1 &last1,
				 iter2 &first2, const iter2 &last2,
				 pred_type pred)
    {
      while (first1 != last1)
	{
	  if (first2 == last2 || !pred (first1, first2))
	    return false;
	  ++first1;
	  ++first2;
	}
      return first2 == last2;
    }

    template<typename t1, typename t2, typename pred_type>
    inline bool container_equal (const t1 &a, const t2 &b, pred_type pred)
    {
      typename t1::const_iterator first1 = a.begin ();
      typename t1::const_iterator last1 = a.end ();
      typename t2::const_iterator first2 = b.begin ();
      typename t2::const_iterator last2 = b.end ();
      return container_equal (first1, last1, first2, last2, pred);
    }

    template<typename t1, typename t2>
    inline bool container_equal (const t1 &a, const t2 &b)
    {
      return container_equal (a, b, deref_equal_to<t1, t2> ());
    }

    template<typename iter>
    inline typename iter::difference_type length (iter i, const iter &end)
    {
      typename iter::difference_type n = 0;
      while (i != end)
	++i, ++n;
      return n;
    }

    template<typename array, typename element = typename array::value_type>
    class indexed_iterator
      : public std::iterator<std::random_access_iterator_tag,
			     typename array::value_type,
			     typename array::difference_type>
    {
    private:
      typedef typename array::size_type index_type;

      array _m_contents;
      index_type _m_idx;

    public:
      indexed_iterator (array contents, index_type idx)
	: _m_contents (contents), _m_idx (idx) {}
      indexed_iterator (const indexed_iterator &i)
	: _m_contents (i._m_contents), _m_idx (i._m_idx) {}

      inline element operator* () const
      {
	return _m_contents[_m_idx];
      }
      template<typename elt>
      inline elt operator* () const
      {
	return _m_contents[_m_idx];
      }
      template<typename elt>
      inline elt *operator-> () const
      {
	return &_m_contents[_m_idx];
      }
      template<typename elt>
      inline elt operator[] (const index_type &n) const
      {
	return _m_contents[_m_idx + n];
      }

      inline indexed_iterator operator+ (const indexed_iterator &i)
      {
	return indexed_iterator (_m_contents, _m_idx + i._m_idx);
      }
      inline indexed_iterator operator+ (const typename array::difference_type
					 &i)
      {
	return indexed_iterator (_m_contents, _m_idx + i);
      }
      inline typename array::difference_type
      operator- (const indexed_iterator &i)
      {
	return _m_idx - i._m_idx;
      }

      inline bool operator== (const indexed_iterator &i)
      {
	return _m_idx == i._m_idx;
      }
      inline bool operator!= (const indexed_iterator &i)
      {
	return _m_idx != i._m_idx;
      }
      inline bool operator< (const indexed_iterator &i)
      {
	return _m_idx < i._m_idx;
      }
      inline bool operator> (const indexed_iterator &i)
      {
	return _m_idx > i._m_idx;
      }
      inline bool operator<= (const indexed_iterator &i)
      {
	return _m_idx <= i._m_idx;
      }
      inline bool operator>= (const indexed_iterator &i)
      {
	return _m_idx >= i._m_idx;
      }

      inline indexed_iterator &operator= (const indexed_iterator &i)
      {
	_m_idx = i._m_idx;
	return *this;
      }
      inline indexed_iterator &operator+= (const index_type &n)
      {
	_m_idx += n;
	return *this;
      }
      inline indexed_iterator &operator-= (const index_type &n)
      {
	_m_idx -= n;
	return *this;
      }

      inline indexed_iterator &operator++ () // prefix
      {
	++_m_idx;
	return *this;
      }
      inline indexed_iterator operator++ (int) // postfix
      {
	return indexed_iterator (_m_contents, _m_idx++);
      }
      inline indexed_iterator &operator-- () // prefix
      {
	--_m_idx;
	return *this;
      }
      inline indexed_iterator operator-- (int) // postfix
      {
	return indexed_iterator (_m_contents, _m_idx--);
      }
    };

    // Pair of some value and its precomputed hash.
    template<typename T>
    class hashed_value
      : public std::pair<size_t, const T>
    {
    private:
      typedef std::pair<size_t, const T> _base;

    public:
      typedef T value_type;

      struct hasher
	: public std::unary_function<hashed_value, size_t>
      {
	inline size_t operator () (const hashed_value &v)
	{
	  return v.first;
	}
      };

      hashed_value (const value_type &v)
	: _base (subr::hash<value_type> (v), v) {}
      hashed_value (const hashed_value &v)
	: _base (v.first, v.second) {}

      bool operator== (const hashed_value &other) const
      {
	return other.first == this->first && other.second == this->second;
      }
    };

    // Set of hashed_value's.
    template<typename value_type>
    class value_set
      : public std::tr1::unordered_set<hashed_value<value_type>,
				       struct hashed_value<value_type>::hasher>
    {
    public:
      typedef hashed_value<value_type> hashed_value_type;

    private:
      typedef std::tr1::unordered_set<hashed_value_type,
				      struct hashed_value_type::hasher> _base;

    public:
      const value_type &add (const value_type &v)
      {
	std::pair<class _base::iterator, bool> p
	  = _base::insert (hashed_value_type (v));
	if (p.second)
	  {
	    // XXX hook for collection: abbrev building, etc.
	  }
	return p.first->second;
      };
    };

    // A container of hashed_value's that itself acts like a hashed_value.
    // The parameter class should be a std::container<hashed_value<something>>.
    template<typename container>
    class hashed_container : public container
    {
    private:
      typedef container _base;
      typedef typename container::value_type elt_type;

    public:
      typedef typename elt_type::value_type value_type;

    private:
      size_t _m_hash;

      inline void set_hash ()
      {
	struct hashit
	{
	  size_t &_m_hash;
	  hashit (size_t &h) : _m_hash (h) {}
	  inline void operator () (const elt_type &p)
	  {
	    subr::hash_combine (_m_hash, p.first);
	  }
	};
	std::for_each (_base::begin (), _base::end (), hashit (_m_hash));
      }

    public:
      friend class hashed_hasher<hashed_container>;
      typedef hashed_hasher<hashed_container> hasher;

      template<typename iterator>
      hashed_container (iterator first, iterator last)
	: _base (first, last), _m_hash (0)
      {
	set_hash ();
      }

      template<typename other_container>
      hashed_container (const other_container &other)
	: _base (other.begin (), other.end ()), _m_hash (0)
      {
	set_hash ();
      }

      bool operator== (const hashed_container &other) const
      {
	return (other._m_hash == _m_hash &&
		other.size () == _base::size ()
		&& std::equal (_base::begin (), _base::end (), other.begin ()));
      }
    };

    // A vector of hashed_value's that itself acts like a hashed_value.
    template<typename value_type>
    struct hashed_vector
      : public hashed_container<std::vector<hashed_value<value_type> > >
    {};

    // An unordered_map of hashed_value's that itself acts like a hashed_value.
    template<typename key_type, typename value_type>
    class hashed_unordered_map
      : public hashed_container<std::tr1::unordered_map<
				  key_type,
				  hashed_value<value_type>,
				  class hashed_value<value_type>::hasher>
      				>
    {};
#if 0
    template<typename key_type, typename value_type>
    class hashed_unordered_map
      : public std::tr1::unordered_map<key_type,
				       hashed_value<value_type>,
				       class hashed_value<value_type>::hasher>
    {
    private:
      typedef std::tr1::unordered_map<key_type,
				      hashed_value<value_type>,
				      class hashed_value<value_type>::hasher>
      _base;

      size_t _m_hash;

      inline void set_hash ()
      {
	struct hashit
	{
	  size_t &_m_hash;
	  hashit (size_t &h) : _m_hash (h) {}

	  inline void operator () (const typename _base::value_type &p)
	  {
	    subr::hash_combine (_m_hash, subr::hash<key_type> (p.first));
	    subr::hash_combine (_m_hash, p.second.first);
	  }
	};
	std::for_each (_base::begin (), _base::end (), hashit (_m_hash));
      }

    public:
      friend class hashed_hasher<hashed_unordered_map>;
      typedef hashed_hasher<hashed_unordered_map> hasher;

      template<typename iterator>
      hashed_unordered_map (iterator first, iterator last)
	: _base (first, last), _m_hash (0)
      {
	set_hash ();
      }

      template<typename container>
      hashed_unordered_map (const container &other)
	: _base (other.begin (), other.end ()), _m_hash (0)
      {
	set_hash ();
      }
    };
#endif

    template<typename T>
    class auto_ref
    {
    private:
      T *_m_ptr;

    public:
      auto_ref (const T &other)
	: _m_ptr (&other)
      {}

      inline operator T& () const
      {
	return *_m_ptr;
      }

      auto_ref (const auto_ref<T> &other)
	: _m_ptr (other._m_ptr)
      {}

      template<typename other>
      inline bool operator== (const auto_ref<other> &x) const
      {
	return *_m_ptr == static_cast<other &> (x);
      }
      template<typename other>
      inline bool operator== (const other &x) const
      {
	return *_m_ptr == x;
      }
      template<typename other>
      inline bool operator!= (const other &x) const
      {
	return !(*this == x);
      }
    };

    /* A wrapped_input_iterator is like an input::const_iterator,
       but *i returns wrapper (*i) instead; wrapper returns element
       (or const element & or something).  */
    template<typename input, typename wrapper,
	     typename element = typename wrapper::result_type>
    class wrapped_input_iterator : public input::const_iterator
    {
    private:
      typedef typename input::const_iterator _base;

      wrapper *_m_wrapper;

    public:
      typedef element value_type;

      inline wrapped_input_iterator (const _base &i, wrapper &w)
	: _base (static_cast<_base> (i)), _m_wrapper (&w)
      {}

      inline wrapped_input_iterator (const wrapped_input_iterator &i)
	: _base (static_cast<_base> (i)), _m_wrapper (i._m_wrapper)
      {}

      inline typename wrapper::result_type operator* () const
      {
	return (*_m_wrapper) (_base::operator* ());
      }
    };
  };
};

#endif	// <elfutils/subr.hh>
