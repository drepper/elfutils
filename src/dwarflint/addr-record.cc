#include "addr-record.h"
#include "misc.h"

size_t
addr_record_find_addr (struct addr_record *ar, uint64_t addr)
{
  size_t a = 0;
  size_t b = ar->size;

  while (a < b)
    {
      size_t i = (a + b) / 2;
      uint64_t v = ar->addrs[i];

      if (v > addr)
	b = i;
      else if (v < addr)
	a = i + 1;
      else
	return i;
    }

  return a;
}

bool
addr_record_has_addr (struct addr_record *ar, uint64_t addr)
{
  if (ar->size == 0
      || addr < ar->addrs[0]
      || addr > ar->addrs[ar->size - 1])
    return false;

  size_t a = addr_record_find_addr (ar, addr);
  return a < ar->size && ar->addrs[a] == addr;
}

void
addr_record_add (struct addr_record *ar, uint64_t addr)
{
  size_t a = addr_record_find_addr (ar, addr);
  if (a >= ar->size || ar->addrs[a] != addr)
    {
      REALLOC (ar, addrs);
      size_t len = ar->size - a;
      memmove (ar->addrs + a + 1, ar->addrs + a, len * sizeof (*ar->addrs));

      ar->addrs[a] = addr;
      ar->size++;
    }
}

void
addr_record_free (struct addr_record *ar)
{
  if (ar != NULL)
    free (ar->addrs);
}

void
ref_record_add (struct ref_record *rr, uint64_t addr, struct where *referrer)
{
  REALLOC (rr, refs);
  struct ref *ref = rr->refs + rr->size++;
  ref->addr = addr;
  ref->who = *referrer;
}

void
ref_record_free (struct ref_record *rr)
{
  if (rr != NULL)
    free (rr->refs);
}
