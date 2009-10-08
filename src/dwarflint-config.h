#ifdef __cplusplus
extern "C"
{
#else
#include <stdbool.h>
#endif

  /* Whole-program options.  */
  extern bool tolerate_nodebug;
  extern bool be_quiet; /* -q */
  extern bool be_verbose; /* -v */
  extern bool be_strict; /* --strict */
  extern bool be_gnu; /* --gnu */
  extern bool be_tolerant; /* --tolerant */
  extern bool show_refs; /* --ref */
  extern bool do_high_level; /* ! --nohl */
  extern bool dump_die_offsets; /* --dump-offsets */

#ifdef __cplusplus
}
#endif
