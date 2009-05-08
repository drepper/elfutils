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
  if (p >= cov->ranges && coalesce->start <= p->start + p->length)
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
coverage_is_covered (struct coverage *cov, uint64_t address)
{
  if (cov->size == 0)
    return false;

  struct cov_range *r = coverage_find (cov, address);
  if (r < cov->ranges + cov->size)
    if (address >= r->start && address < r->start + r->length)
      return true;
  if (r > cov->ranges)
    {
      --r;
      if (address >= r->start && address < r->start + r->length)
	return true;
    }
  return false;
}

bool
coverage_find_holes (struct coverage *cov, uint64_t start, uint64_t length,
		     bool (*hole)(uint64_t start, uint64_t length, void *data),
		     void *data)
{
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

void
coverage_free (struct coverage *cov)
{
  free (cov->ranges);
}
