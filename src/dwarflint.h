#ifndef DWARFLINT_HL_H
#define DWARFLINT_HL_H

#include "../libdw/libdw.h"

#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  /* Entry points for high-level checks.  */

  /* Check that .debug_aranges and .debug_ranges match.  */
  extern bool check_matching_ranges (Dwarf *dwarf);


  /* Functions and data structures describing location in Dwarf.  */

  enum section_id
  {
    sec_invalid = 0,
    sec_info,
    sec_abbrev,
    sec_aranges,
    sec_pubnames,
    sec_pubtypes,
    sec_str,
    sec_loc,
    sec_locexpr, /* Not a section, but a portion of file that contains a
		    location expression.  */
    sec_ranges,
    sec_rel,
    sec_rela,
  };

  struct where
  {
    enum section_id section;
    uint64_t addr1; // E.g. a CU offset.
    uint64_t addr2; // E.g. a DIE address.
    uint64_t addr3; // E.g. an attribute.
    struct where *ref; // Related reference, e.g. an abbrev related to given DIE.
    struct where *next; // Hierarchically superior location.
  };

# define WHERE(SECTION, NEXT)						\
  ((struct where)							\
    {(SECTION), (uint64_t)-1, (uint64_t)-1, (uint64_t)-1, NULL, NEXT})

  extern const char *where_fmt (const struct where *wh, char *ptr);
  extern void where_fmt_chain (const struct where *wh, const char *severity);
  extern void where_reset_1 (struct where *wh, uint64_t addr);
  extern void where_reset_2 (struct where *wh, uint64_t addr);
  extern void where_reset_3 (struct where *wh, uint64_t addr);


  /* Functions and data structures for emitting various types of
     messages.  */

  enum message_category
  {
    mc_none      = 0,

    /* Severity: */
    mc_impact_1  = 0x1, // no impact on the consumer
    mc_impact_2  = 0x2, // still no impact, but suspicious or worth mentioning
    mc_impact_3  = 0x4, // some impact
    mc_impact_4  = 0x8, // high impact
    mc_impact_all= 0xf, // all severity levels
    mc_impact_2p = 0xe, // 2+
    mc_impact_3p = 0xc, // 3+

    /* Accuracy:  */
    mc_acc_bloat     = 0x10, // unnecessary constructs (e.g. unreferenced strings)
    mc_acc_suboptimal= 0x20, // suboptimal construct (e.g. lack of siblings)
    mc_acc_all       = 0x30, // all accuracy options

    /* Various: */
    mc_error     = 0x40,  // turn the message into an error

    /* Area: */
    mc_leb128    = 0x100,  // ULEB/SLEB storage
    mc_abbrevs   = 0x200,  // abbreviations and abbreviation tables
    mc_die_rel   = 0x400,  // DIE relationship
    mc_die_other = 0x800,  // other messages related to DIEs
    mc_info      = 0x1000, // messages related to .debug_info, but not particular DIEs
    mc_strings   = 0x2000, // string table
    mc_aranges   = 0x4000, // address ranges table
    mc_elf       = 0x8000, // ELF structure, e.g. missing optional sections
    mc_pubtables = 0x10000,  // table of public names/types
    mc_pubtypes  = 0x20000,  // .debug_pubtypes presence
    mc_loc       = 0x40000,  // messages related to .debug_loc
    mc_ranges    = 0x80000,  // messages related to .debug_ranges
    mc_other     = 0x100000, // messages unrelated to any of the above
    mc_all       = 0xffffff00, // all areas
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

  extern void wr_format_leb128_message (int st, struct where *wh,
					const char *what);

  extern void wr_message_padding_0 (enum message_category category,
				    struct where *wh,
				    uint64_t start, uint64_t end);

  extern void wr_message_padding_n0 (enum message_category category,
				     struct where *wh,
				     uint64_t start, uint64_t end);


  /* Functions and data structures for handling of address range
     coverage.  We use that to find holes of unused bytes in DWARF
     string table.  */

  typedef uint_fast32_t coverage_emt_type;
  static const size_t coverage_emt_size = sizeof (coverage_emt_type);
  static const size_t coverage_emt_bits = 8 * sizeof (coverage_emt_type);

  struct coverage
  {
    size_t alloc;
    uint64_t size;
    coverage_emt_type *buf;
  };

  struct section_coverage
  {
    Elf_Scn *scn;
    GElf_Shdr shdr;
    struct coverage cov;
    bool hit; /* true if COV is not pristine.  */
  };

  struct coverage_map
  {
    Elf *elf;
    size_t size;
    size_t alloc;
    bool allow_overlap;
    struct section_coverage *scos;
  };

  void coverage_init (struct coverage *ar, uint64_t size);
  void coverage_add (struct coverage *ar, uint64_t begin, uint64_t end);
  bool coverage_is_covered (struct coverage *ar, uint64_t address);
  bool coverage_find_holes (struct coverage *ar,
			    bool (*cb)(uint64_t begin, uint64_t end, void *data),
			    void *data);
  void coverage_free (struct coverage *ar);

  void section_coverage_init (struct section_coverage *sco, Elf_Scn *scn,
			      GElf_Shdr *shdr);
  bool coverage_map_init (struct coverage_map *coverage_map, Elf *elf,
			  Elf64_Xword mask, bool allow_overlap);
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
    unsigned align;
    void *data;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->data has to point at d_buf of section in question.  */
  bool found_hole (uint64_t begin, uint64_t end, void *data);

  struct coverage_map_hole_info
  {
    struct hole_info info;
    Elf *elf;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->info.data has to be NULL, it is used by the callback.  */
  bool coverage_map_found_hole (uint64_t begin, uint64_t end,
				struct section_coverage *sco, void *data);


#ifdef __cplusplus
}
#endif

#endif/*DWARFLINT_HL_H*/
