#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "dwarflint-coverage.h"
#include <stdbool.h>
#include <assert.h>
#include <system.h>
#include <string.h>
#include <inttypes.h>

static struct cov_range *
coverage_find (struct coverage *cov, uint64_t start)
{
  assert (cov->size > 0);

  size_t a = 0;
  size_t b = cov->size;

  while (a < b)
    {
      size_t i = (a + b) / 2;
      struct cov_range *r = cov->ranges + i;

      if (r->start > start)
	b = i;
      else if (r->start < start)
	a = i + 1;
      else
	return r;
    }

  return cov->ranges + a;
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
coverage_is_covered (struct coverage *cov, uint64_t start, uint64_t length)
{
  assert (length > 0);

  if (cov->size == 0)
    return false;

  struct cov_range *r = coverage_find (cov, start);
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

bool
coverage_is_overlap (struct coverage *cov, uint64_t start, uint64_t length)
{
  if (length == 0 || cov->size == 0)
    return false;

  uint64_t end = start + length;
  bool overlaps (struct cov_range *r)
  {
    return (start >= r->start && start < r->start + r->length)
      || (end > r->start && end <= r->start + r->length)
      || (start < r->start && end > r->start + r->length);
  }

  struct cov_range *r = coverage_find (cov, start);

  if (r < cov->ranges + cov->size && overlaps (r))
    return true;

  if (r > cov->ranges)
    return overlaps (r - 1);

  return false;
}

bool
coverage_find_holes (struct coverage *cov, uint64_t start, uint64_t length,
		     bool (*hole)(uint64_t start, uint64_t length, void *data),
		     void *data)
{
  if (length == 0)
    return true;

  if (cov->size == 0)
    return hole (start, length, data);

  uint64_t end (size_t i)
  {
    return cov->ranges[i].start + cov->ranges[i].length;
  }

  if (start < cov->ranges[0].start)
    if (!hole (start, cov->ranges[0].start - start, data))
      return false;

  for (size_t i = 0; i < cov->size - 1; ++i)
    {
      uint64_t end_i = end (i);
      if (!hole (end_i, cov->ranges[i+1].start - end_i, data))
	return false;
    }

  if (start + length > end (cov->size - 1))
    {
      uint64_t end_last = end (cov->size - 1);
      return hole (end_last, start + length - end_last, data);
    }

  return true;
}

bool
coverage_find_ranges (struct coverage *cov,
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

struct coverage *
coverage_clone (struct coverage *cov)
{
  struct coverage *ret = xmalloc (sizeof (*ret));
  WIPE (*ret);
  coverage_add_all (ret, cov);
  return ret;
}

void
coverage_add_all (struct coverage *cov, struct coverage *other)
{
  for (size_t i = 0; i < other->size; ++i)
    coverage_add (cov, other->ranges[i].start, other->ranges[i].length);
}

bool
coverage_remove_all (struct coverage *cov, struct coverage *other)
{
  bool ret = false;
  for (size_t i = 0; i < other->size; ++i)
    if (coverage_remove (cov, other->ranges[i].start, other->ranges[i].length))
      ret = true;
  return ret;
}
