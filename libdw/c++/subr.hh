/* Private helper classes for elfutils -*- C++ -*- interfaces.

 */

#ifndef _ELFUTILS_SUBR_HH
#define _ELFUTILS_SUBR_HH	1

#include <iterator>
#include <functional>
#include <cstring>
#include <iostream>
#include <sstream>

namespace elfutils
{
  namespace subr
  {
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
  };
};

#endif	// <elfutils/subr.hh>
