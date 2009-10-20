#include "dwarflint-checks-low.hh"
#include "dwarflint-config.h"
#include "c++/dwarf"

template<class T>
class highlevel_check
  : public check<highlevel_check<T> >
{
  ::Dwarf *_m_handle;

public:
  elfutils::dwarf dw;

  // xxx this will throw an exception on <c++/dwarf> or <libdw.h>
  // failure.  We need to catch it and convert to check_base::failed.
  explicit highlevel_check (dwarflint &lint)
    : _m_handle (dwarf_begin_elf (lint.elf (), DWARF_C_READ, NULL))
    , dw (_m_handle)
  {
    if (!do_high_level)
      throw check_base::unscheduled;
  }

  ~highlevel_check ()
  {
    dwarf_end (_m_handle);
  }
};
