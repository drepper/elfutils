/* Low-level checking of .debug_loc and .debug_range.
   Copyright (C) 2009, 2010, 2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "checks.hh"
#include "sections_i.hh"
#include "check_debug_info_i.hh"
#include "messages.hh"
#include "coverage.hh"
#include "dwarf_version_i.hh"

class loc_range_locus
  : public locus
{
  locus const &_m_refby;
  Dwarf_Off _m_offset;
  section_id _m_sec;

public:
  loc_range_locus (section_id sec, locus const &refby, Dwarf_Off offset = -1)
    : _m_refby (refby)
    , _m_offset (offset)
    , _m_sec ((assert (sec == sec_loc || sec == sec_ranges), sec))
  {}

  std::string format (bool brief) const;
};

struct section_coverage
{
  struct sec *sec;
  struct coverage cov;
  bool hit; /* true if COV is not pristine.  */
  bool warn; /* dwarflint should emit a warning if a coverage appears
		in this section */
  section_coverage (struct sec *a_sec, bool a_warn);
};

struct coverage_map
{
  struct elf_file *elf;
  std::vector<section_coverage> scos;
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
				locus const &loc);

void check_range_relocations (locus const &loc,
			      enum message_category cat,
			      struct elf_file const *file,
			      GElf_Sym *begin_symbol,
			      GElf_Sym *end_symbol,
			      const char *description);
