/* Initialization of high-level check context
   Copyright (C) 2009 Red Hat, Inc.
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

  // xxx this will throw an exception on <c++/dwarf> or <libdw.h>
  // failure.  We need to catch it and convert to check_base::failed.

#include "checks-high.hh" // xxx rename

namespace
{
  inline bool failed (void *ptr) { return ptr == NULL; }
  inline bool failed (int i) { return i < 0; }

  template <class T>
  inline T
  throw_if_failed (T x, char const *msg,
		   char const *(*errmsgcb) (int) = NULL)
  {
    if (unlikely (failed (x)))
      {
	std::stringstream ss;
	ss << msg;
	if (errmsgcb != NULL)
	  ss << ": " << errmsgcb (-1);
	ss << '.';
	wr_error () << ss.str () << std::endl;
	throw check_base::failed ();
      }
    return x;
  }

  Dwfl *open_dwfl () __attribute__ ((nonnull, malloc));
  Dwarf *open_dwarf (Dwfl *dwfl, int fd) __attribute__ ((nonnull, malloc));

  Dwfl *
  open_dwfl ()
  {
    static class my_callbacks
      : public Dwfl_Callbacks
    {
      // Stub libdwfl callback, only the ELF handle already open is ever used.
      static int
      find_no_debuginfo (Dwfl_Module *mod __attribute__ ((unused)),
			 void **userdata __attribute__ ((unused)),
			 const char *modname __attribute__ ((unused)),
			 Dwarf_Addr base __attribute__ ((unused)),
			 const char *file_name __attribute__ ((unused)),
			 const char *debuglink_file __attribute__ ((unused)),
			 GElf_Word debuglink_crc __attribute__ ((unused)),
			 char **debuginfo_file_name __attribute__ ((unused)))
      {
	return -1;
      }

    public:
      my_callbacks ()
      {
	section_address = dwfl_offline_section_address;
	find_debuginfo = find_no_debuginfo;
      }
    } cbs;

    return throw_if_failed (dwfl_begin (&cbs),
			    "Couldn't initialize DWFL");
  }

  char const *
  mystrerror (int i)
  {
    if (i == -1)
      i = errno;
    return strerror (i);
  }

  Dwarf *
  open_dwarf (Dwfl *dwfl, char const *fname, int fd)
  {
    dwfl_report_begin (dwfl);

    // Dup FD for dwfl to consume.
    int dwfl_fd
      = throw_if_failed (dup (fd), "Error: dup", mystrerror);

    Dwfl_Module *mod
      = throw_if_failed (dwfl_report_offline (dwfl, fname, fname, dwfl_fd),
			 "Couldn't add DWFL module", dwfl_errmsg);
    dwfl_report_end (dwfl, NULL, NULL);
    Dwarf_Addr bias;
    throw_if_failed (dwfl_module_getelf (mod, &bias),
		     "Couldn't open ELF.", dwfl_errmsg);
    return throw_if_failed (dwfl_module_getdwarf (mod, &bias),
			    "Couldn't obtain DWARF descriptor", dwfl_errmsg);
  }

  elfutils::dwarf
  open_hl_dwarf (Dwarf *dw)
  {
    try
      {
	return dw;
      }
    catch (...)
      {
	wr_error ()
	  << "Couldn't initialize high-level DWARF descriptor." << std::endl;
	throw check_base::failed ();
      }
  }
}

open_highlevel_dwarf::open_highlevel_dwarf (dwarflint &lint)
  : _m_dwfl (open_dwfl ())
  , _m_dw (open_dwarf (_m_dwfl, lint.fname (), lint.fd ()))
  , dw (open_hl_dwarf (_m_dw))
{}

open_highlevel_dwarf::~open_highlevel_dwarf ()
{
  dwfl_end (_m_dwfl);
}
