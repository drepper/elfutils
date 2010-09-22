/* Implementation of coverage analysis.

   Copyright (C) 2008, 2009, 2010 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "coverage.hh"
#include "pri.hh"
#include "misc.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

namespace
{
  template <class X>
  decltype (((X *)0)->ranges)
  coverage_find (X *cov, uint64_t start)
  {
    assert (cov->size > 0);

    size_t a = 0;
    size_t b = cov->size;

    while (a < b)
      {
	size_t i = (a + b) / 2;
	cov_range const *r = cov->ranges + i;

	if (r->start > start)
	  b = i;
	else if (r->start < start)
	  a = i + 1;
	else
	  return cov->ranges + i;
      }

    return cov->ranges + a;
  }
}

void
coverage_add (struct coverage *cov, uint64_t start, uint64_t length)
{
  if (length == 0)
    return;

  struct cov_range nr = (struct cov_range){start, length};
  if (cov->size == 0)
    {
      REALLOC (cov, ranges);
      cov->ranges[cov->size++] = nr;
      return;
    }

  struct cov_range *r = coverage_find (cov, start);

  struct cov_range *insert = &nr;
  struct cov_range *coalesce = &nr;
  struct cov_range *end = cov->ranges + cov->size;

  // Coalesce with previous range?
  struct cov_range *p = r - 1;
  if (r > cov->ranges && coalesce->start <= p->start + p->length)
    {
      uint64_t coalesce_end = coalesce->start + coalesce->length;
      if (coalesce_end > p->start + p->length)
	{
	  p->length = coalesce_end - p->start;
	  coalesce = p;
	}
      else
	coalesce = NULL;
      insert = NULL;
    }

  // Coalesce with one or more following ranges?
  if (coalesce != NULL && coalesce != end)
    {
      p = r;
      while (p != end && coalesce->start + coalesce->length >= p->start)
	{
	  uint64_t p_end = p->start + p->length;
	  if (p_end > coalesce->start + coalesce->length)
	    coalesce->length = p_end - coalesce->start;
	  if (insert != NULL)
	    {
	      *p = *insert;
	      insert = NULL;
	      coalesce = p;
	      assert (p == r);
	      ++r; // when doing memory moves, don't overwrite this range
	    }
	  ++p;
	}
      if (p > r)
	{
	  size_t rem = cov->size - (p - cov->ranges);
	  memmove (r, p, sizeof (*cov->ranges) * rem);
	  cov->size -= p - r;
	}
    }

  if (insert != NULL)
    {
      size_t rem = end - r;
      size_t idx = r - cov->ranges;
      REALLOC (cov, ranges);
      r = cov->ranges + idx;

      cov->size++;
      if (rem > 0)
	memmove (r + 1, r, sizeof (*cov->ranges) * rem);
      *r = nr;
    }
}

bool
coverage_remove (struct coverage *cov, uint64_t begin, uint64_t length)
{
  uint64_t end = begin + length;
  if (cov->size == 0 || begin == end)
    return false;

  struct cov_range *r = coverage_find (cov, begin);
  struct cov_range *erase_begin = NULL, *erase_end = r; // end exclusive
  bool overlap = false;

  // Cut from previous range?
  struct cov_range *p = r - 1;
  if (r > cov->ranges && begin < p->start + p->length)
    {
      uint64_t r_end = p->start + p->length;
      // Do we cut the beginning of the range?
      if (begin == p->start)
	p->length = end >= r_end ? 0 : r_end - end;
      else
	{
	  p->length = begin - p->start;
	  // Do we shoot a hole in that range?
	  if (end < r_end)
	    {
	      coverage_add (cov, end, r_end - end);
	      return true;
	    }
	}

      overlap = true;
      if (p->length == 0)
	erase_begin = p;
    }

  if (erase_begin == NULL)
    erase_begin = r;

  // Cut from next range?
  while (r < cov->ranges + cov->size
	 && r->start < end)
    {
      overlap = true;
      if (end >= r->start + r->length)
	{
	  ++erase_end;
	  ++r;
	}
      else
	{
	  uint64_t end0 = r->start + r->length;
	  r->length = end0 - end;
	  r->start = end;
	  assert (end0 == r->start + r->length);
	}
    }

  // Did we cut out anything completely?
  if (erase_end > erase_begin)
    {
      struct cov_range *cov_end = cov->ranges + cov->size;
      size_t rem = cov_end - erase_end;
      if (rem > 0)
	memmove (erase_begin, erase_end, sizeof (*cov->ranges) * rem);
      cov->size -= erase_end - erase_begin;
    }

  return overlap;
}

bool
coverage_is_covered (struct coverage const *cov,
		     uint64_t start, uint64_t length)
{
  assert (length > 0);

  if (cov->size == 0)
    return false;

  struct cov_range const *r = coverage_find (cov, start);
  uint64_t end = start + length;
  if (r < cov->ranges + cov->size)
    if (start >= r->start)
      return end <= r->start + r->length;

  if (r > cov->ranges)
    {
      --r;
      return end <= r->start + r->length;
    }

  return false;
}

namespace
{
  bool overlaps (uint64_t start, uint64_t end,
		 struct cov_range const *r)
  {
    return (start >= r->start && start < r->start + r->length)
      || (end > r->start && end <= r->start + r->length)
      || (start < r->start && end > r->start + r->length);
  }
}

char *
range_fmt (char *buf, size_t buf_size, uint64_t start, uint64_t end)
{
  std::stringstream ss;
  ss << pri::range (start, end);
  std::string s = ss.str ();
  strncpy (buf, s.c_str (), buf_size);
  return buf;
}

bool
coverage_is_overlap (struct coverage const *cov,
		     uint64_t start, uint64_t length)
{
  if (length == 0 || cov->size == 0)
    return false;

  uint64_t end = start + length;

  struct cov_range const *r = coverage_find (cov, start);

  if (r < cov->ranges + cov->size && overlaps (start, end, r))
    return true;

  if (r > cov->ranges)
    return overlaps (start, end, r - 1);

  return false;
}

bool
coverage_find_holes (struct coverage const *cov,
		     uint64_t start, uint64_t length,
		     bool (*hole)(uint64_t start, uint64_t length, void *data),
		     void *data)
{
  if (length == 0)
    return true;

  if (cov->size == 0)
    return hole (start, length, data);

  if (start < cov->ranges[0].start)
    if (!hole (start, cov->ranges[0].start - start, data))
      return false;

  for (size_t i = 0; i < cov->size - 1; ++i)
    {
      uint64_t end_i = cov->ranges[i].end ();
      if (!hole (end_i, cov->ranges[i+1].start - end_i, data))
	return false;
    }

  if (start + length > cov->back ().end ())
    {
      uint64_t end_last = cov->back ().end ();
      return hole (end_last, start + length - end_last, data);
    }

  return true;
}

bool
coverage_find_ranges (struct coverage const *cov,
		      bool (*cb)(uint64_t start, uint64_t length, void *data),
		      void *data)
{
  for (size_t i = 0; i < cov->size; ++i)
    if (!cb (cov->ranges[i].start, cov->ranges[i].length, data))
      return false;

  return true;
}

void
coverage_free (struct coverage *cov)
{
  free (cov->ranges);
}

coverage *
coverage_clone (struct coverage const *cov)
{
  coverage *ret = (coverage *)xmalloc (sizeof (*ret));
  WIPE (*ret);
  coverage_add_all (ret, cov);
  return ret;
}

void
coverage_add_all (struct coverage *__restrict__ cov,
		  struct coverage const *__restrict__ other)
{
  for (size_t i = 0; i < other->size; ++i)
    coverage_add (cov, other->ranges[i].start, other->ranges[i].length);
}

bool
coverage_remove_all (struct coverage *__restrict__ cov,
		     struct coverage const *__restrict__ other)
{
  bool ret = false;
  for (size_t i = 0; i < other->size; ++i)
    if (coverage_remove (cov, other->ranges[i].start, other->ranges[i].length))
      ret = true;
  return ret;
}

bool
cov::_format_base::fmt (uint64_t start, uint64_t length)
{
  if (_m_seen)
    _m_os << _m_delim;
  _m_os << pri::range (start, start + length);
  _m_seen = true;
  return true;
}

bool
cov::_format_base::wrap_fmt (uint64_t start, uint64_t length, void *data)
{
  _format_base *self = static_cast <_format_base *> (data);
  return self->fmt (start, length);
}

cov::_format_base::_format_base (std::string const &delim)
  : _m_delim (delim),
    _m_seen (false)
{}

cov::format_ranges::format_ranges (coverage const &cov,
				   std::string const &delim)
  : _format_base (delim)
{
  coverage_find_ranges (&cov, &wrap_fmt, this);
}

cov::format_holes::format_holes (coverage const &cov,
				 uint64_t start, uint64_t length,
				 std::string const &delim)
  : _format_base (delim)
{
  coverage_find_holes (&cov, start, length, &wrap_fmt, this);
}
