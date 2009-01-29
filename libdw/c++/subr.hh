/* Private helper classes for elfutils -*- C++ -*- interfaces.

 */

#ifndef _ELFUTILS_SUBR_HH
#define _ELFUTILS_SUBR_HH	1

#include <iterator>

namespace elfutils
{
  namespace subr
  {

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
      inline indexed_iterator operator- (const indexed_iterator &i)
      {
	return indexed_iterator (_m_contents, _m_idx - i._m_idx);
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
