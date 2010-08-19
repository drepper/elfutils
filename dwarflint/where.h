#ifndef DWARFLINT_WHERE_H
#define DWARFLINT_WHERE_H

#include "section_id.h"

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
#include <iosfwd>
extern "C"
{
#endif

  enum where_formatting
  {
    wf_plain = 0, /* Default formatting for given section.  */
    wf_cudie,
  };

  struct where
  {
    enum section_id section;
    enum where_formatting formatting;
    uint64_t addr1; // E.g. a CU offset.
    uint64_t addr2; // E.g. a DIE address.
    uint64_t addr3; // E.g. an attribute.
    struct where const *ref; // Related reference, e.g. an abbrev
			     // related to given DIE.
    struct where const *next; // For forming "caused-by" chains.
  };

# define WHERE(SECTION, NEXT)						\
  ((struct where)							\
   {(SECTION), wf_plain,						\
    (uint64_t)-1, (uint64_t)-1, (uint64_t)-1,				\
    NULL, NEXT})

  extern const char *where_fmt (const struct where *wh,	char *ptr);
  extern void where_fmt_chain (const struct where *wh, const char *severity);
  extern void where_reset_1 (struct where *wh, uint64_t addr);
  extern void where_reset_2 (struct where *wh, uint64_t addr);
  extern void where_reset_3 (struct where *wh, uint64_t addr);

#ifdef __cplusplus
}

#include <iostream>

inline std::ostream &
operator << (std::ostream &os, where const &wh)
{
  os << where_fmt (&wh, NULL);
  return os;
}

#endif

#endif//DWARFLINT_WHERE_H
