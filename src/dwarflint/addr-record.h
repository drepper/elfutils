#ifndef DWARFLINT_ADDR_RECORD_H
#define DWARFLINT_ADDR_RECORD_H

#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

/* Functions and data structures for address record handling.  We use
   that to check that all DIE references actually point to an existing
   die, not somewhere mid-DIE, where it just happens to be
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

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_ADDR_RECORD_H
