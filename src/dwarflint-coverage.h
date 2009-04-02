#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define REALLOC(A, BUF)						\
  do {								\
    typeof ((A)) _a = (A);					\
    if (_a->size == _a->alloc)					\
      {								\
	if (_a->alloc == 0)					\
	  _a->alloc = 8;					\
	else							\
	  _a->alloc *= 2;					\
	_a->BUF = xrealloc (_a->BUF,				\
			    sizeof (*_a->BUF) * _a->alloc);	\
      }								\
  } while (0)

#define WIPE(OBJ) memset (&OBJ, 0, sizeof (OBJ))

/* Functions and data structures for handling of address range
 coverage.  We use that to find holes of unused bytes in DWARF
 string table.  */

struct cov_range
{
  uint64_t start;
  uint64_t length;
};

struct coverage
{
  struct cov_range *ranges;
  size_t size;
  size_t alloc;
};

void coverage_add (struct coverage *ar, uint64_t begin, uint64_t end);
bool coverage_is_covered (struct coverage *ar, uint64_t address);
bool coverage_find_holes (struct coverage *cov, uint64_t start, uint64_t length,
			  bool (*cb)(uint64_t start, uint64_t length, void *data),
			  void *data);
void coverage_free (struct coverage *ar);
