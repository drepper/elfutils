/* Private helper classes for elfutils -*- C++ -*- interfaces.

 */

#ifndef _ELFUTILS_SUBR_HH
#define _ELFUTILS_SUBR_HH	1

#include <iterator>
#include <functional>
#include <cstring>
#include <cassert>
#include <iostream>
#include <sstream>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <vector>
#include <deque>
#include <stack>
#include <set>
#include <algorithm>
#include <utility>
#include <stdexcept>

namespace elfutils
{
  namespace subr
  {
    template<typename container_type, typename op_type>
    inline void for_each (container_type &container, op_type op)
    {
      std::for_each (container.begin (), container.end (), op);
    }

    template<typename T>
    struct hash : public T::hasher {};

    template<typename T>
    static inline size_t hash_this (const T &v)
    {
      return hash<T> () (v);
    }

    template <typename T>
    inline void hash_combine (size_t &seed, const T &v)
    {
      seed ^= hash_this (v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <typename T1, typename T2>
    inline void hash_combine (size_t &seed, const std::pair<T1,T2> &v)
    {
      hash_combine (seed, v.first);
      hash_combine (seed, v.second);
    }

    template<typename T, typename B>
    struct base_hasher : public std::unary_function<T, size_t>
    {
      size_t operator () (const T &v) const
      {
	return hash_this<B> (v);
      }
    };

    template<typename T1, typename T2>
    struct cast_hasher : public std::unary_function<T1, size_t>
    {
      inline size_t operator () (const T1 &x) const
      {
	return hash_this (static_cast<T2> (x));
      }
    };

    template<typename T, T magic>
    struct integer_hash : public std::unary_function<T, size_t>
    {
      inline size_t operator () (const T &x) const
      {
	return x * magic;
      }
    };
    template<>
    struct hash<unsigned int>
      : public integer_hash<unsigned int, 0x9e370001U>
    {};
    template<>
    struct hash<uint64_t>
      : public integer_hash<uint64_t, 0x9e37fffffffc0001ULL>
    {};

    template<>
    struct hash<int> : public cast_hasher<int, unsigned int> {};
    template<>
    struct hash<uint8_t> : public cast_hasher<uint8_t, unsigned int> {};
    template<>
    struct hash<bool> : public cast_hasher<bool, unsigned int> {};

    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2> >
      : public std::unary_function<std::pair<T1, T2>, size_t>
    {
      inline size_t operator () (const std::pair<T1, T2> &x) const
      {
	size_t h = 0;
	hash_combine (h, x);
	return h;
      }
    };

    template<typename T,
	     void (*combiner) (size_t &, const typename T::value_type &)
	       = hash_combine<typename T::value_type>,
	     size_t initial_hash = 0>
    class container_hasher
      : public std::unary_function<T, size_t>
    {
    private:
      struct hasher
      {
	size_t &_m_hash;
	inline explicit hasher (size_t &hash) : _m_hash (hash) {}
	inline void operator () (const typename T::value_type &x)
	{
	  return (*combiner) (_m_hash, x);
	}
      };
    public:
      inline size_t operator () (const T &x) const
      {
	size_t hash = initial_hash;
	for_each (x, hasher (hash));
	return hash;
      }
    };

    template<typename T>
    struct hash<std::vector<T> >
      : public container_hasher<std::vector<T> >
    {
    };

    template<>
    struct hash<std::string>
      : public std::tr1::hash<std::string>
    {
    };

    template<class T>
    struct hashed_hasher
      : public std::unary_function<T, size_t>
    {
      size_t operator () (const T &v) const
      {
	return v._m_hash;
      }
    };

    template<typename string>
    struct name_equal : public std::binary_function<const char *, string, bool>
    {
      template<typename mystring>
      inline bool operator () (const mystring &me, const string &you) const
      {
	return you == me;
      }
    };

    // Explicit specialization.
    template<>
    struct name_equal<const char *>
      : public std::binary_function<const char *, const char *, bool>
    {
      bool operator () (const char *me, const char *you) const
      {
	return !strcmp (me, you);
      }
      template<typename mystring>
      inline bool operator () (const mystring &me, const char *you) const
      {
	return me == you;
      }
    };

    static inline std::string hex_string (int code)
    {
      std::ostringstream os;
      os << std::hex << std::showbase << code;
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

    // This is like std::equal_to but for comparing two different types.
    template<typename t1, typename t2>
    struct equal_to : public std::binary_function<t1, t2, bool>
    {
      inline bool operator () (const t1 &a, const t2 &b) const
      {
	return a == b;
      }
    };

    /* On a single type, our equal_to is like std::equal_to, but
       we short-circuit for the case of matching pointers.  */
    template<typename T>
    struct equal_to<T, T> : public std::binary_function<T, T, bool>
    {
      inline bool operator () (const T &a, const T &b) const
      {
	return &a == &b || a == b;
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
			       const typename t2::const_iterator &b) const
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
    inline bool container_equal (const t1 &a, const t2 &b, pred_type pred,
				 typename t1::size_type skip = 0)
    {
      typename t1::const_iterator first1 = a.begin ();
      typename t1::const_iterator last1 = a.end ();
      typename t2::const_iterator first2 = b.begin ();
      typename t2::const_iterator last2 = b.end ();
      while (skip-- > 0)
	{
	  if (first1 == last1)
	    return first2 == last2;
	  if (first2 == last2)
	    return first1 == last1;
	  ++first1;
	  ++first2;
	}
      return container_equal (first1, last1, first2, last2, pred);
    }

    template<typename t1, typename t2>
    inline bool container_equal (const t1 &a, const t2 &b)
    {
      return container_equal (a, b, deref_equal_to<t1, t2> ());
    }

    template<typename t1, typename t2>
    inline bool container_tail_equal (const t1 &a, const t2 &b,
				      typename t1::size_type skip = 0)
    {
      return container_equal (a, b, deref_equal_to<t1, t2> (), skip);
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

      inline indexed_iterator operator+ (const indexed_iterator &i) const
      {
	return indexed_iterator (_m_contents, _m_idx + i._m_idx);
      }
      inline indexed_iterator operator+ (const typename array::difference_type
					 &i) const
      {
	return indexed_iterator (_m_contents, _m_idx + i);
      }
      inline typename array::difference_type
      operator- (const indexed_iterator &i) const
      {
	return _m_idx - i._m_idx;
      }

      inline bool operator== (const indexed_iterator &i) const
      {
	return _m_idx == i._m_idx;
      }
      inline bool operator!= (const indexed_iterator &i) const
      {
	return _m_idx != i._m_idx;
      }
      inline bool operator< (const indexed_iterator &i) const
      {
	return _m_idx < i._m_idx;
      }
      inline bool operator> (const indexed_iterator &i) const
      {
	return _m_idx > i._m_idx;
      }
      inline bool operator<= (const indexed_iterator &i) const
      {
	return _m_idx <= i._m_idx;
      }
      inline bool operator>= (const indexed_iterator &i) const
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
	inline size_t operator () (const hashed_value &v) const
	{
	  return v.first;
	}
      };

      hashed_value (const value_type &v)
	: _base (hash_this (v), v) {}
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
      const value_type *add (const value_type &v)
      {
	std::pair<class _base::iterator, bool> p
	  = _base::insert (hashed_value_type (v));
	if (p.second)
	  {
	    // XXX hook for collection: abbrev building, etc.
	  }
	return &p.first->second;
      };

      template<typename input>
      const value_type *add (const input &v)
      {
	return add (value_type (v));
      }

      template<typename input, typename arg_type>
      const value_type *add (const input &v, arg_type &arg)
      {
	return add (value_type (v, arg));
      }
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

    protected:
      size_t _m_hash;

      inline void set_hash ()
      {
	_m_hash = container_hasher<container> () (*this);
      }

    public:
      friend class hashed_hasher<hashed_container>;
      typedef hashed_hasher<hashed_container> hasher;

      template<typename iterator>
      hashed_container (iterator first, iterator last)
	: _base (first, last)
      {
	set_hash ();
      }

      template<typename other_container>
      hashed_container (const other_container &other)
	: _base (other.begin (), other.end ())
      {
	set_hash ();
      }

      bool operator== (const hashed_container &other) const
      {
	return (other._m_hash == _m_hash &&
		other.size () == _base::size ()
		&& std::equal (_base::begin (), _base::end (), other.begin (),
			       equal_to<elt_type, elt_type> ()));
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
	    hash_combine (_m_hash, hash_this (p.first));
	    hash_combine (_m_hash, p.second.first);
	  }
	};
	for_each (static_cast<_base &> (*this), hashit (_m_hash));
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
    template<typename input, class wrapper,
	     typename element = typename wrapper::result_type>
    class wrapped_input_iterator : public input::const_iterator
    {
    private:
      typedef typename input::const_iterator _base;

      wrapper _m_wrapper;

    public:
      typedef element value_type;

      inline wrapped_input_iterator ()
	: _base ()
      {}

      template<typename arg_type>
      inline wrapped_input_iterator (const _base &i, const arg_type &arg)
	: _base (static_cast<_base> (i)), _m_wrapper (arg)
      {}

      inline wrapped_input_iterator (const wrapped_input_iterator &i)
	: _base (static_cast<_base> (i)), _m_wrapper (i._m_wrapper)
      {}

      inline typename wrapper::result_type operator* () const
      {
	return _m_wrapper (_base::operator* ());
      }

      inline element *operator-> () const
      {
	return &(_m_wrapper (_base::operator* ()));
      }

      inline wrapped_input_iterator &operator++ () // prefix
      {
	_base::operator++ ();
	return *this;
      }
      inline wrapped_input_iterator operator++ (int) // postfix
      {
	wrapped_input_iterator pre = *this;
	++*this;
	return pre;
      }
      inline wrapped_input_iterator &operator-- () // prefix
      {
	_base::operator-- ();
	return *this;
      }
      inline wrapped_input_iterator operator-- (int) // postfix
      {
	wrapped_input_iterator pre = *this;
	--*this;
	return pre;
      }

      inline const _base &base () const
      {
	return *this;
      }

      inline _base &base ()
      {
	return *this;
      }

      template<typename container = input>
      struct copy
      {
	template<typename arg_type>
	inline container operator () (const input &in,
				      const arg_type &arg = arg_type ())
	{
	  return container (wrapped_input_iterator (in.begin (), arg),
			    wrapped_input_iterator (in.end (), arg));
	}
      };
    };

    /* A wrapped_input_container provides begin and end methods that
       wrap the real container's iterators with wrapped_input_iterator.  */
    template<typename input, class wrapper,
	     typename element = typename wrapper::result_type>
    class wrapped_input_container
    {
    private:
      const input &_m_container;
      wrapper _m_wrapper;

    public:
      typedef wrapped_input_iterator<input, wrapper, element> const_iterator;
      typedef const_iterator iterator;

      template<typename arg_type>
      inline wrapped_input_container (const input &container,
				      const arg_type &arg)
	: _m_container (container), _m_wrapper (arg)
      {}

      inline const_iterator begin () const
      {
	return const_iterator (_m_container.begin (), _m_wrapper);
      }

      inline const_iterator end () const
      {
	return const_iterator (_m_container.end (), _m_wrapper);
      }

      static inline bool ordered ()
      {
	return input::ordered ();
      }
    };

    /* An iterator adapter for use in iterator-based constructors.
       collectify (iterator) yields an iterator on input where *i
       constructs output::value_type (input::value_type v, collector).  */
    template<typename input, typename output, typename arg_type>
    struct argifier
      : public std::unary_function<typename input::const_iterator,
				   typename output::iterator> // not really
    {
      typedef typename input::const_iterator inny;
      typedef typename output::iterator outty;
      typedef typename input::value_type inlet;
      typedef typename output::value_type outlet;

      /* Wrapper worker passed to wrapped_input_iterator.
	 This object holds the collector pointer.  */
      struct maker
	: public std::unary_function<inlet, outlet>
      {
	const arg_type _m_arg;

	inline maker (const arg_type &c)
	  : _m_arg (c)
	{}

	inline maker (const maker &m)
	  : _m_arg (m._m_arg)
	{}

	inline outlet operator () (const inlet &x) const
	{
	  return outlet (x, _m_arg);
	}
      } _m_maker;

      explicit inline argifier (const arg_type &c)
	: _m_maker (c)
      {}

      typedef wrapped_input_iterator<input, maker> result_type;

      inline result_type operator () (const inny &i)
      {
	return result_type (i, _m_maker);
      }
    };

    template<typename input, typename output, typename arg_type>
    static inline typename argifier<input, output, arg_type>::result_type
    argify (const typename input::const_iterator &in, const arg_type &arg)
    {
      return argifier<input, output, arg_type> (arg) (in);
    }

    template<typename input, typename output, typename arg_type>
    struct argifier2nd
      : public std::unary_function<typename input::const_iterator,
				   typename output::iterator>
    {
      typedef typename input::const_iterator inny;
      typedef typename output::iterator outty;
      typedef typename input::value_type inlet;
      typedef typename output::value_type outlet;

      /* Wrapper worker passed to wrapped_input_iterator.
	 This object holds the collector pointer.  */
      struct pair_maker
	: public argifier<input, output, arg_type>::maker
      {
	typedef typename argifier<input, output, arg_type>::maker maker;

	inline pair_maker (const arg_type &c) : maker (c) {}
	inline pair_maker (const pair_maker &m) : maker (m) {}

	inline outlet operator () (const inlet &x) const
	{
	  return std::make_pair (x.first,
				 typename outlet::second_type (x.second,
							       this->_m_arg));
	}
      } _m_maker;

      explicit inline argifier2nd (const arg_type &c)
	: _m_maker (c)
      {}

      typedef wrapped_input_iterator<input, pair_maker> const_iterator;

      inline const_iterator operator () (const inny &i)
      {
	return const_iterator (i, _m_maker);
      }
    };

    template<typename input, typename output, typename arg_type>
    static inline typename argifier2nd<input, output, arg_type>::const_iterator
    argify2nd (const typename input::const_iterator &in, const arg_type &arg)
    {
      return argifier2nd<input, output, arg_type> (arg) (in);
    }

    /* A guard object is intended to be ephemeral, existing solely to be
       destroyed in exception paths where it was not cleared explicitly.
       In that case, it calls tracker::soiled ().

       For convenience, it can be constructed from a tracker reference or
       pointer, or default-constructed and then filled.  It's fillable by
       calling the guard object as a function, passing it the tracker
       reference or pointer, which it passes through on return:

	       guard<tracker> g;
	       use (g (t));
	       g.clear ();

       This first calls T.start ().  When G goes out of scope,
       it calls T.abort () iff G.clear () was never called.  */

    template<typename tracker>
    class guard
    {
    private:
      tracker *_m_tracker;

      inline void start ()
      {
	_m_tracker->start ();
      }

    public:
      inline guard (tracker *t)
	: _m_tracker (t)
      {
	start ();
      }

      inline guard (tracker &t)
	: _m_tracker (&t)
      {
	start ();
      }

      inline guard ()
	: _m_tracker (NULL)
      {}

      inline tracker *operator () (tracker *t)
      {
	_m_tracker = t;
	start ();
	return t;
      }

      inline tracker &operator () (tracker &t)
      {
	_m_tracker = &t;
	start ();
	return t;
      }

      inline operator tracker * () const
      {
	return _m_tracker;
      }

      inline operator tracker & () const
      {
	return *_m_tracker;
      }

      inline void clear ()
      {
	_m_tracker = NULL;
      }

      inline ~guard ()
      {
	if (unlikely (_m_tracker != NULL))
	  _m_tracker->abort ();
      }
    };

    struct nothing
    {
    };

    // Class instead of function so it can be a friend.
    struct create_container
    {
      struct setter
      {
	template<typename in_iter, typename out_iter, typename arg_type>
	inline void operator () (const out_iter &out, const in_iter &in,
				 bool, arg_type arg) const
	{
	  out->set (*in, arg);
	}
      };

      template<typename container, typename input, typename arg_type,
	       typename hook_type = const setter>
      inline create_container (container *me, const input &other,
			       arg_type &arg, hook_type &hook = hook_type ())
      	{
	  typename input::const_iterator in = other.begin ();
	  bool last = in == other.end ();
	  while (!last)
	    {
	      /* Don't copy-construct the entry from *in here because that
		 copies it again into the list and destroys the first copy.  */
	      me->push_back (typename container::value_type ());
	      typename container::iterator out = --me->end ();
	      const typename input::const_iterator here = in++;
	      last = in == other.end ();
	      hook (out, here, last, arg);
	    }
	}
    };

    template<typename T>
    struct is : public std::equal_to<T>
    {
      bool operator () (const T &a, const T &b) const
      {
	return a.is (b);
      }
    };

    template<typename T>
    struct is<T *> : public std::equal_to<T *>
    {
      bool operator () (const T *a, const T *b) const
      {
	return a == b || a->is (*b);
      }
    };

#if 0				// unused
    template<typename T1, typename T2>
    struct is<std::pair<T1, T2> > : public std::equal_to<std::pair<T1, T2> >
    {
      bool operator () (const std::pair<T1, T2> &a,
			const std::pair<T1, T2> &b) const
      {
	return (is<T1> () (a.first, b.first)
		&& is<T2> () (a.second, b.second));
      }
    };
#endif

    template<typename T>
    struct identity_set
      : public std::tr1::unordered_set<T, typename T::hasher, is<T> >
    {
    };

    template<typename key_type, typename mapped_type>
    struct identity_map
      : public std::tr1::unordered_map<key_type, mapped_type,
				       typename key_type::hasher,
				       is<key_type> >
    {};

    /* This is like an unordered_set, but the equality predicate cannot
       be fixed statically.  Instead, each insertion call must pass in
       the specific predicate to match against existing elements for
       that insertion.  */
    template<typename T, typename hasher_type = hash<T> >
    class dynamic_equality_set
    {
    public:
      typedef T value_type;
      typedef size_t hash_type;
      typedef std::deque<T> bucket_type;

    private:
      typedef std::tr1::unordered_map<hash_type, bucket_type> map_type;

      map_type _m_map;
      hasher_type _m_hasher;

    public:
      template<typename match_type>
      inline const value_type *
      add (const value_type &candidate, match_type &match)
      {
	bucket_type &bucket = _m_map[_m_hasher (candidate)];

	for (typename bucket_type::iterator i = bucket.begin ();
	     i != bucket.end ();
	     ++i)
	  {
	    const value_type &elt = *i;
	    if (match (elt, candidate))
	      // We have a winner!
	      return &elt;
	  }

	// No matches: new element.
	bucket.push_back (candidate);
	return &(bucket.back ());
      }

      // Unclear why you didn't just use a normal identity_set then!
      inline const value_type *add (const value_type &candidate)
      {
	is<value_type> equalator;
	return add (candidate, equalator);
      }

      template<typename reporter>
      inline void hash_stats (std::ostream &out, const char *name,
			      const reporter &report_collisions) const
      {
	size_t collisions = 0;
	size_t empty_buckets = 0;
	size_t total = 0;
	size_t largest = 0;
	for (typename map_type::const_iterator i = _m_map.begin ();
	     i != _m_map.end ();
	     ++i)
	  {
	    if (i->second.empty ())
	      ++empty_buckets;
	    else
	      {
		size_t n = i->second.size () - 1;
		collisions += n;
		if (n > 0)
		  report_collisions (i->first, i->second);
	      }
	    if (i->second.size () > largest)
	      largest = i->second.size ();
	    total += i->second.size ();
	  }
	out << name << ": " << total << ", "
	    << collisions << " collisions, "
	    << largest << " in largest bucket";
	if (empty_buckets > 0)
	  out << ", " << empty_buckets << " empty buckets\n";
	else
	  out << "\n";
      }
    };

    template<typename set_type>
    inline void container_hash_stats (std::ostream &out, const char *name,
				      const set_type &set)
    {
      std::set<size_t> hashes;
      for (typename set_type::const_iterator i = set.begin ();
	   i != set.end ();
	   ++i)
	hashes.insert (hash_this (*i));
      out << name << ": " << set.size () << ", "
	  << hashes.size () << " hashes = "
	  << (set.size () - hashes.size ()) << " collisions\n";
    }

    /* sharing_stack<T> is like std::stack<T>, but copies share list
       tails to reduce the memory footprint.  Any non-const call to the
       top method copies the top element so it's no longer shared.
       So be sure to use const calls for any non-modifying access.
       The top_const method is a short-hand for a const access to
       a non-const container.  */
    template<typename T>
    class sharing_stack
    {
    public:
      typedef T value_type;
      typedef size_t size_type;
      typedef value_type &reference;
      typedef const value_type &const_reference;

    protected:
      class element
      {
      private:
	friend class sharing_stack;
	value_type _m_value;
	element *_m_next;
	unsigned int _m_count;

	inline element &operator= (const element &)
	{
	  throw std::logic_error ("cannot assign");
	}

	inline element ()
	{
	  throw std::logic_error ("cannot default-construct");
	}

      public:
	inline unsigned int count () const
	{
	  return _m_count;
	}

	inline operator value_type & ()
	{
	  return _m_value;
	}

	inline operator const value_type & () const
	{
	  return _m_value;
	}

	inline element (const value_type &value, element *tail)
	  : _m_value (value), _m_next (tail), _m_count (1)
	{
	  if (_m_next != NULL)
	    _m_next->acquire ();
	}

	inline element (const element &other)
	  : _m_value (other._m_value), _m_next (other._m_next), _m_count (1)
	{
	  if (_m_next != NULL)
	    _m_next->acquire ();
	}

	inline ~element ()
	{
	  if (_m_next != NULL)
	    _m_next->release ();
	}

	inline void acquire ()
	{
	  assert (_m_count > 0);
	  ++_m_count;
	}

	inline void release ()
	{
	  assert (_m_count > 0);
	  if (--_m_count == 0)
	    delete this;
	}

	inline const element *next () const
	{
	  return _m_next;
	}

	inline bool shared () const
	{
	  assert (_m_count > 0);
	  return _m_count > 1;
	}
      };

      element *_m_head;
      size_type _m_size;

      inline void init (element *head, size_type n)
      {
	if (head == NULL)
	  assert (n == 0);
	else
	  head->acquire ();
	_m_head = head;
	_m_size = n;
      }

    public:
      inline void clear ()
      {
	if (_m_head == NULL)
	  assert (_m_size == 0);
	else
	  {
	    _m_head->release ();
	    _m_head = NULL;
	    _m_size = 0;
	  }
      }

      inline bool empty () const
      {
	return _m_head == NULL;
      }

      inline size_type size () const
      {
	return _m_size;
      }

      inline void push (const value_type &value)
      {
	element *old = _m_head;
	_m_head = new element (value, _m_head);
	++_m_size;
	if (old != NULL)
	  old->release ();
      }

      inline void pop ()
      {
	--_m_size;
	element *tail = _m_head->_m_next;
	if (tail != NULL)
	  tail->acquire ();
	_m_head->release ();
	_m_head = tail;
      }

      inline const value_type &top () const
      {
	assert (_m_head != NULL);
	return *_m_head;
      }

      inline const value_type &const_top () const
      {
	return top ();
      }

      inline value_type &top ()
      {
	element *prev = _m_head;
	if (prev->shared ())
	  {
	    _m_head = new element (*prev);
	    prev->release ();
	  }
	assert (!_m_head->shared ());
	return *_m_head;
      }

      inline sharing_stack ()
	: _m_head (NULL), _m_size (0)
      {}

      inline sharing_stack (const sharing_stack &other)
      {
	init (other._m_head, other._m_size);
      }

      inline ~sharing_stack ()
      {
	clear ();
      }

      inline sharing_stack &operator= (const sharing_stack &other)
      {
	if (&other != this)
	  {
	    clear ();
	    init (other._m_head, other._m_size);
	  }
	return *this;
      }

      inline void swap (sharing_stack &other)
      {
	std::swap (_m_head, other._m_head);
	std::swap (_m_size, other._m_size);
      }

      class const_reverse_iterator
	: public std::iterator<std::input_iterator_tag, value_type>
      {
      private:
	const element *_m_elt;

	friend class sharing_stack;
	inline const_reverse_iterator (const element *elt)
	  : _m_elt (elt)
	{}

      public:
	inline const value_type &operator* () const
	{
	  return *_m_elt;
	}

	inline bool operator== (const const_reverse_iterator &other) const
	{
	  return _m_elt == other._m_elt;
	}
	inline bool operator!= (const const_reverse_iterator &other) const
	{
	  return !(*this == other);
	}

	inline const_reverse_iterator &operator++ () // prefix
	{
	  _m_elt = _m_elt->next ();
	  return *this;
	}
	inline const_reverse_iterator operator++ (int) // postfix
	{
	  const const_reverse_iterator old = *this;
	  ++*this;
	  return old;
	}
      };

      inline const_reverse_iterator rbegin () const
      {
	return const_reverse_iterator (_m_head);
      }

      inline const_reverse_iterator rend () const
      {
	return const_reverse_iterator (NULL);
      }

      template<typename other_value_type, typename pred_type>
      inline bool equal (const sharing_stack<other_value_type> &other,
			 pred_type &pred, size_type skip = 0) const
      {
	if (other.size () != size ())
	  return false;

	const_reverse_iterator a = rbegin ();
	typename sharing_stack<other_value_type>::const_reverse_iterator b
	  = other.rbegin ();

	std::advance (a, skip);
	std::advance (b, skip);

	return std::equal (a, rend (), b, pred);
      }

      template<typename other_value_type>
      inline bool operator== (const sharing_stack<other_value_type> &other)
	const
      {
	equal_to<value_type, other_value_type> equalator;
	return equal (other, equalator);
      }

      template<typename other_value_type>
      inline bool operator!= (const sharing_stack<other_value_type> &other)
	const
      {
	return !(*this == other);
      }
    };

    // Compatible with sharing_stack, but actually a std::stack.
    template<typename T, typename container_type = std::deque<T> >
    struct stackish
      : public std::stack<T, container_type>
    {
      inline const T &const_top () const
      {
	return this->top ();
      }

      inline void clear ()
      {
	this->c.clear ();
      }

      typedef typename container_type::const_reverse_iterator
      const_reverse_iterator;

      inline const_reverse_iterator rbegin () const
      {
	return this->c.rbegin ();
      }

      inline const_reverse_iterator rend () const
      {
	return this->c.rend ();
      }

      template<typename other_value_type, typename other_container_type,
	       typename pred_type>
      inline bool
      equal (const stackish<other_value_type, other_container_type> &other,
	     pred_type &pred, typename container_type::size_type skip = 0) const
      {
	if (other.size () != this->size ())
	  return false;

	typename container_type::const_reverse_iterator a = this->rbegin ();
	typename other_container_type::const_reverse_iterator b
	  = other.rbegin ();

	std::advance (a, skip);
	std::advance (b, skip);

	return std::equal (a, this->rend (), b, pred);
      }
    };

    /* This is a dummy you can template/syntactically use in
       place of std::cout et al for disabled debugging spew.  */
    struct nostream
    {
      inline const nostream &
      operator<< (std::ostream &(*) (std::ostream &)) const
      {
	return *this;
      }

      template<typename arg>
      inline const nostream &
      operator<< (const arg &) const
      {
	return *this;
      }
    };
  };
};

#endif	// <elfutils/subr.hh>
