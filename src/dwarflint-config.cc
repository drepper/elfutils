#include "dwarflint-config.h"

/* If true, we accept silently files without debuginfo.  */
bool tolerate_nodebug = false;

/* True if no message is to be printed if the run is succesful.  */
bool be_quiet = false; /* -q */
bool be_verbose = false; /* -v */
bool be_strict = false; /* --strict */
bool be_gnu = false; /* --gnu */
bool be_tolerant = false; /* --tolerant */
bool show_refs = false; /* --ref */
bool do_high_level = true; /* ! --nohl */
bool dump_die_offsets = false; /* --dump-offsets */
