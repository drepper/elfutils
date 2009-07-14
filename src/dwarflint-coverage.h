#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct coverage *coverage_clone (struct coverage *cov) __attribute__ ((malloc));
void coverage_free (struct coverage *cov);

void coverage_add (struct coverage *cov, uint64_t start, uint64_t length);
void coverage_add_all (struct coverage *cov, struct coverage *other);

/* Returns true if something was actually removed, false if whole
   range falls into hole in coverage.  */
bool coverage_remove (struct coverage *cov, uint64_t start, uint64_t length);

/* Returns true if something was actually removed, false if whole
   range falls into hole in coverage.  */
bool coverage_remove_all (struct coverage *cov, struct coverage *other);

/* Returns true if whole range ADDRESS/LENGTH is covered by COV.
   LENGTH may not be zero.  */
bool coverage_is_covered (struct coverage *cov, uint64_t start, uint64_t length);

/* Returns true if at least some of the range ADDRESS/LENGTH is
   covered by COV.  Zero-LENGTH range never overlaps.  */
bool coverage_is_overlap (struct coverage *cov, uint64_t start, uint64_t length);

bool coverage_find_holes (struct coverage *cov, uint64_t start, uint64_t length,
			  bool (*cb)(uint64_t start, uint64_t length, void *data),
			  void *data);
bool coverage_find_ranges (struct coverage *cov,
			  bool (*cb)(uint64_t start, uint64_t length, void *data),
			  void *data);
