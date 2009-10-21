#ifndef DWARFLINT_MISC_H
#define DWARFLINT_MISC_H

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
	_a->BUF = (typeof (_a->BUF))				\
	  xrealloc (_a->BUF,					\
		    sizeof (*_a->BUF) * _a->alloc);		\
      }								\
  } while (0)

#define WIPE(OBJ) memset (&OBJ, 0, sizeof (OBJ))

#ifdef __cplusplus
# define IF_CPLUSPLUS(X) X
#else
# define IF_CPLUSPLUS(X) /*X*/
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "../lib/system.h"

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_MISC_H
