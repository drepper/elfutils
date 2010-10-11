/* Low-level checking of .debug_loc and .debug_range.
   Copyright (C) 2009, 2010 Red Hat, Inc.
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

#include "checks.hh"
#include "sections.ii"
#include "check_debug_info.ii"
#include "messages.hh"
#include "coverage.hh"
#include "dwarf_version.ii"

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

class check_debug_ranges
  : public check<check_debug_ranges>
{
  section<sec_ranges> *_m_sec_ranges;
  check_debug_info *_m_info;
  coverage _m_cov;

public:
  static checkdescriptor const *descriptor ();

  coverage const &cov () const { return _m_cov; }
  check_debug_ranges (checkstack &stack, dwarflint &lint);
  ~check_debug_ranges ();
};

class check_debug_loc
  : public check<check_debug_loc>
{
  section<sec_loc> *_m_sec_loc;
  check_debug_info *_m_info;

public:
  static checkdescriptor const *descriptor ();
  check_debug_loc (checkstack &stack, dwarflint &lint);
};

struct hole_info
{
  enum section_id section;
  enum message_category category;
  void *data;
  unsigned align;
};

/* DATA has to be a pointer to an instance of struct hole_info.
   DATA->data has to point at d_buf of section in question.  */
bool found_hole (uint64_t start, uint64_t length, void *data);

bool check_location_expression (dwarf_version const *ver,
				elf_file const &file,
				struct read_ctx *parent_ctx,
				struct cu *cu,
				uint64_t init_off,
				struct relocation_data *reloc,
				size_t length,
				struct where *wh);

void check_range_relocations (enum message_category cat,
			      struct where *where,
			      struct elf_file const *file,
			      GElf_Sym *begin_symbol,
			      GElf_Sym *end_symbol,
			      const char *description);
