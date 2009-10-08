#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
# define IF_CPLUSPLUS(X) X
extern "C"
{
#else
# define IF_CPLUSPLUS(X) /*X*/
#endif

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

  extern const char *where_fmt (const struct where *wh, char *ptr IF_CPLUSPLUS (= NULL));
  extern void where_fmt_chain (const struct where *wh, const char *severity);
  extern void where_reset_1 (struct where *wh, uint64_t addr);
  extern void where_reset_2 (struct where *wh, uint64_t addr);
  extern void where_reset_3 (struct where *wh, uint64_t addr);

#ifdef __cplusplus
}
#endif
