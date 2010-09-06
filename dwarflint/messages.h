/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

#ifndef DWARFLINT_MESSAGES_H
#define DWARFLINT_MESSAGES_H

#include "where.h"
#include "libdw.h"

#ifdef __cplusplus
# define IF_CPLUSPLUS(X) X
# include <string>
extern "C"
{
#endif

#define MESSAGE_CATEGORIES						\
  /* Severity: */							\
  MC (impact_1,  0)  /* no impact on the consumer */			\
  MC (impact_2,  1)  /* still no impact, but suspicious or worth mentioning */ \
  MC (impact_3,  2)  /* some impact */					\
  MC (impact_4,  3)  /* high impact */					\
									\
  /* Accuracy:  */							\
  MC (acc_bloat, 4)  /* unnecessary constructs (e.g. unreferenced strings) */ \
  MC (acc_suboptimal, 5) /* suboptimal construct (e.g. lack of siblings) */ \
									\
  /* Various: */							\
  MC (error,     6)      /* turn the message into an error */		\
									\
  /* Area: */								\
  MC (leb128,    7)  /* ULEB/SLEB storage */				\
  MC (abbrevs,   8)  /* abbreviations and abbreviation tables */	\
  MC (die_rel,   9)  /* DIE relationship */				\
  MC (die_other, 10) /* other messages related to DIEs */		\
  MC (info,      11) /* messages related to .debug_info, but not particular DIEs */ \
  MC (strings,   12) /* string table */					\
  MC (aranges,   13) /* address ranges table */				\
  MC (elf,       14) /* ELF structure, e.g. missing optional sections */ \
  MC (pubtables, 15) /* table of public names/types */			\
  MC (pubtypes,  16) /* .debug_pubtypes presence */			\
  MC (loc,       17) /* messages related to .debug_loc */		\
  MC (ranges,    18) /* messages related to .debug_ranges */		\
  MC (line,      19) /* messages related to .debug_line */		\
  MC (reloc,     20) /* messages related to relocation handling */	\
  MC (header,    21) /* messages related to header portions in general */ \
  MC (other,     31) /* messages unrelated to any of the above */

  enum message_category
  {
    mc_none      = 0,

#define MC(CAT, ID)\
    mc_##CAT = 1u << ID,
    MESSAGE_CATEGORIES
#undef MC
  };

  struct message_term
  {
    /* Given a term like A && !B && C && !D, we decompose it thus: */
    unsigned long positive; /* non-zero bits for plain predicates */
    unsigned long negative; /* non-zero bits for negated predicates */

#ifdef __cplusplus
    message_term (unsigned long pos, unsigned long neg)
      : positive (pos), negative (neg)
    {}
    std::string str () const;
#endif
  };

  struct message_criteria
  {
    struct message_term *terms;
    size_t size;
    size_t alloc;

#ifdef __cplusplus
    message_criteria ()
      : terms (NULL), size (0), alloc (0)
    {}
    ~message_criteria ()
    {
      free (terms);
    }

    void operator |= (message_term const &term);
    void operator &= (message_term const &term);
    std::string str () const;
#endif
  };

#ifdef __cplusplus
  message_criteria operator ! (message_term const &);
#endif

  extern void wr_error (const struct where *wh, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

  extern void wr_warning (const struct where *wh, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

  extern void wr_message (unsigned long category, const struct where *wh,
			  const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));

  extern void wr_format_padding_message (unsigned long category,
					 struct where const *wh,
					 uint64_t start, uint64_t end,
					 char const *kind);

  extern void wr_format_leb128_message (struct where const *where,
					const char *what,
					const char *purpose,
					const unsigned char *begin,
					const unsigned char *end);

  extern void wr_message_padding_0 (unsigned long category,
				    struct where const *wh,
				    uint64_t start, uint64_t end);

  extern void wr_message_padding_n0 (unsigned long category,
				     struct where const *wh,
				     uint64_t start, uint64_t end);

  extern bool message_accept (struct message_criteria const *cri,
			      unsigned long cat);


  extern unsigned error_count;

  /* Messages that are accepted (and made into warning).  */
  extern struct message_criteria warning_criteria;

  /* Accepted (warning) messages, that are turned into errors.  */
  extern struct message_criteria error_criteria;

#ifdef __cplusplus
}

inline message_category
cat (message_category c1,
     message_category c2,
     message_category c3 = mc_none,
     message_category c4 = mc_none)
{
  return static_cast<message_category> (c1 | c2 | c3 | c4);
}

std::ostream &wr_warning (where const &wh);
std::ostream &wr_warning ();
std::ostream &wr_error (where const &wh);
std::ostream &wr_error ();
std::ostream &wr_message (where const &wh, message_category cat);
std::ostream &wr_message (message_category cat);
#endif

#endif//DWARFLINT_MESSAGES_H
