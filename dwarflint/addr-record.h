#ifndef DWARFLINT_ADDR_RECORD_H
#define DWARFLINT_ADDR_RECORD_H

#include <stdlib.h>
#include <stdint.h>
#include "where.h"
#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  /* Functions and data structures for address record handling.  We
     use that to check that all DIE references actually point to an
     existing die, not somewhere mid-DIE, where it just happens to be
     interpretable as a DIE.  */

  struct addr_record
  {
    size_t size;
    size_t alloc;
    uint64_t *addrs;
  };

  size_t addr_record_find_addr (struct addr_record *ar, uint64_t addr);
  bool addr_record_has_addr (struct addr_record *ar, uint64_t addr);
  void addr_record_add (struct addr_record *ar, uint64_t addr);
  void addr_record_free (struct addr_record *ar);

  /* Functions and data structures for reference handling.  Just like
     the above, we use this to check validity of DIE references.
     Unlike the above, this is not stored as sorted set, but simply as
     an array of records, because duplicates are unlikely.  */

  struct ref
  {
    uint64_t addr; // Referree address
    struct where who;  // Referrer
  };

  struct ref_record
  {
    size_t size;
    size_t alloc;
    struct ref *refs;
  };

  void ref_record_add (struct ref_record *rr, uint64_t addr,
		       struct where *referrer);
  void ref_record_free (struct ref_record *rr);

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_ADDR_RECORD_H
