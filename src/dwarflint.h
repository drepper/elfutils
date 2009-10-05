/* Pedantic checking of DWARF files.
   Copyright (C) 2008,2009 Red Hat, Inc.
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

#ifndef DWARFLINT_HL_H
#define DWARFLINT_HL_H

#include "../libdw/libdw.h"
#include "../libebl/libebl.h"

#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  /* Entry points for high-level checks.  */

  struct hl_ctx;

  /* Check that .debug_aranges and .debug_ranges match.  */
  extern struct hl_ctx *hl_ctx_new (Elf *elf);
  extern void hl_ctx_delete (struct hl_ctx *hlctx);
  extern bool check_matching_ranges (struct hl_ctx *hlctx);
  extern bool check_expected_trees (struct hl_ctx *hlctx);


  /* Functions and data structures describing location in Dwarf.  */

#define DEBUGINFO_SECTIONS \
  SEC (info)		   \
  SEC (abbrev)		   \
  SEC (aranges)		   \
  SEC (pubnames)	   \
  SEC (pubtypes)	   \
  SEC (str)		   \
  SEC (line)		   \
  SEC (loc)		   \
  SEC (mac)		   \
  SEC (ranges)

  enum section_id
  {
    sec_invalid = 0,

    /* Debuginfo sections:  */
#define SEC(n) sec_##n,
    DEBUGINFO_SECTIONS
    count_debuginfo_sections,
#undef SEC

    /* Non-debuginfo sections:  */
    sec_rel = count_debuginfo_sections,
    sec_rela,

    /* Non-sections:  */
    sec_locexpr,	/* Not a section, but a portion of file that
			   contains a location expression.  */
    rel_value,		/* For relocations, this denotes that the
			   relocation is applied to taget value, not a
			   section offset.  */
    rel_address,	/* Same as above, but for addresses.  */
    rel_exec,		/* Some as above, but we expect EXEC bit.  */
  };

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
    struct where *ref; // Related reference, e.g. an abbrev related to given DIE.
    struct where *next; // For forming "caused-by" chains.
  };

# define WHERE(SECTION, NEXT)						\
  ((struct where)							\
   {(SECTION), wf_plain,						\
    (uint64_t)-1, (uint64_t)-1, (uint64_t)-1,				\
    NULL, NEXT})

  extern const char *where_fmt (const struct where *wh, char *ptr);
  extern void where_fmt_chain (const struct where *wh, const char *severity);
  extern void where_reset_1 (struct where *wh, uint64_t addr);
  extern void where_reset_2 (struct where *wh, uint64_t addr);
  extern void where_reset_3 (struct where *wh, uint64_t addr);


  /* Functions and data structures for emitting various types of
     messages.  */

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

  extern void wr_error (const struct where *wh, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

  extern void wr_warning (const struct where *wh, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

  extern void wr_message (enum message_category category, const struct where *wh,
			  const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));

  extern void wr_format_padding_message (enum message_category category,
					 struct where *wh,
					 uint64_t start, uint64_t end,
					 char *kind);

  extern void wr_format_leb128_message (struct where *where, const char *what,
					const char *purpose,
					const unsigned char *begin,
					const unsigned char *end);

  extern void wr_message_padding_0 (enum message_category category,
				    struct where *wh,
				    uint64_t start, uint64_t end);

  extern void wr_message_padding_n0 (enum message_category category,
				     struct where *wh,
				     uint64_t start, uint64_t end);

# include "dwarflint-coverage.h"

  extern char *range_fmt (char *buf, size_t buf_size, uint64_t start, uint64_t end);

  struct relocation
  {
    uint64_t offset;
    uint64_t addend;
    int symndx;
    int type;
    bool invalid;		/* Whether this one relocation should be
				   ignored.  Necessary so that we don't
				   double-report invalid & missing
				   relocation.  */
  };

  struct relocation_data
  {
    Elf_Data *symdata;		/* Symbol table associated with this
				   relocation section.  */
    size_t type;		/* SHT_REL or SHT_RELA.  */

    struct relocation *rel;	/* Array of relocations.  May be NULL
				   if there are no associated
				   relocation data.  */
    size_t size;
    size_t alloc;
    size_t index;		/* Current index. */
  };

  struct sec
  {
    GElf_Shdr shdr;
    struct relocation_data rel;
    Elf_Scn *scn;
    const char *name;

    Elf_Data *data;	/* May be NULL if data in this section are
			   missing or not substantial.  */
    enum section_id id;
  };

  struct elf_file
  {
    GElf_Ehdr ehdr;	/* Header of underlying Elf.  */
    Elf *elf;
    Ebl *ebl;

    struct sec *sec;	/* Array of sections.  */
    size_t size;
    size_t alloc;

    /* Pointers into SEC above.  Maps section_id to section.  */
    struct sec *debugsec[count_debuginfo_sections];

    bool addr_64;	/* True if it's 64-bit Elf.  */
    bool other_byte_order; /* True if the file has a byte order
			      different from the host.  */
  };

  struct section_coverage
  {
    struct sec *sec;
    struct coverage cov;
    bool hit; /* true if COV is not pristine.  */
    bool warn; /* dwarflint should emit a warning if a coverage
		  appears in this section */
  };

  struct coverage_map
  {
    struct elf_file *elf;
    struct section_coverage *scos;
    size_t size;
    size_t alloc;
    bool allow_overlap;
  };

  void section_coverage_init (struct section_coverage *sco,
			      struct sec *sec, bool warn);
  bool coverage_map_init (struct coverage_map *coverage_map,
			  struct elf_file *elf,
			  Elf64_Xword mask,
			  Elf64_Xword warn_mask,
			  bool allow_overlap);
  void coverage_map_add (struct coverage_map *coverage_map,
			 uint64_t address, uint64_t length,
			 struct where *where, enum message_category cat);
  bool coverage_map_find_holes (struct coverage_map *coverage_map,
				bool (*cb) (uint64_t, uint64_t,
					    struct section_coverage *, void *),
				void *user);
  void coverage_map_free (struct coverage_map *coverage_map);


  struct hole_info
  {
    enum section_id section;
    enum message_category category;
    void *data;
    unsigned align;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->data has to point at d_buf of section in question.  */
  bool found_hole (uint64_t begin, uint64_t end, void *data);

  struct coverage_map_hole_info
  {
    struct elf_file *elf;
    struct hole_info info;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->info.data has to be NULL, it is used by the callback.  */
  bool coverage_map_found_hole (uint64_t begin, uint64_t end,
				struct section_coverage *sco, void *data);


#ifdef __cplusplus
}
#endif

#endif/*DWARFLINT_HL_H*/
