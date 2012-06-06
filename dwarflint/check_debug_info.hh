/* Low-level checking of .debug_info.
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

#ifndef DWARFLINT_CHECK_DEBUG_INFO_HH
#define DWARFLINT_CHECK_DEBUG_INFO_HH

#include <libdw.h>
#include "addr-record.hh"
#include "elf_file_i.hh"
#include "coverage.hh"
#include "checks.hh"
#include "check_debug_abbrev_i.hh"
#include "check_debug_line_i.hh"
#include "check_debug_aranges_i.hh"
#include "sections_i.hh"
#include "die_locus.hh"

typedef ref_T<die_locus> ref;
typedef ref_record_T<die_locus> ref_record;

struct cu_head
{
  Dwarf_Off offset;
  Dwarf_Off size;               // Size of this CU.
  Dwarf_Off head_size;          // Size from begin to 1st byte of CU.
  Dwarf_Off total_size;         // size + head_size

  int offset_size;              // Offset size in this CU.
  Dwarf_Off abbrev_offset;      // Abbreviation section that this CU uses.
  int version;                  // CU version
  int address_size;             // Address size in bytes on the target machine.

  cu_locus where;

  explicit cu_head (Dwarf_Off a_offset)
    : offset (a_offset)
    , size (0)
    , head_size (0)
    , total_size (0)
    , offset_size (0)
    , abbrev_offset (0)
    , version (0)
    , address_size (0)
    , where (a_offset)
  {}
};

struct cu
{
  struct cu *next;              // For compatibility with C level.
                                // xxx will probably go away eventually
  cu_head const *head;
  uint64_t cudie_offset;
  uint64_t low_pc;              // DW_AT_low_pc value of CU DIE, -1 if not present.
  ::ref stmt_list;
  addr_record die_addrs;        // Addresses where DIEs begin in this CU.
  ref_record die_refs;          // DIE references into other CUs from this CU.
  ref_record loc_refs;          // references into .debug_loc from this CU.
  ref_record range_refs;        // references into .debug_ranges from this CU.
  ref_record decl_file_refs;    // values of DW_AT_decl_file in this CU.
  bool has_arange;              // Whether we saw arange section pointing at this CU.
  bool has_pubnames;            // Likewise for pubnames.
  bool has_pubtypes;            // Likewise for pubtypes.

  cu ()
    : next (NULL)
    , head (NULL)
    , cudie_offset (0)
    , low_pc (0)
    , has_arange (false)
    , has_pubnames (false)
    , has_pubtypes (false)
  {}
};

/** The pass for reading basic .debug_info data -- the layout of
    sections and their headers.  */
class read_cu_headers
  : public check<read_cu_headers>
{
  section<sec_info> *_m_sec_info;

public:
  static checkdescriptor const *descriptor ();
  std::vector<cu_head> const cu_headers;
  read_cu_headers (checkstack &stack, dwarflint &lint);
};

/** The pass for in-depth structural analysis of .debug_info.  */
class check_debug_info
  : public check<check_debug_info>
{
  section<sec_info> *_m_sec_info;
  section<sec_str> *_m_sec_str;
  elf_file const &_m_file;
  check_debug_abbrev *_m_abbrevs;
  read_cu_headers *_m_cu_headers;

  // Abbreviation table with that offset had user(s) that failed
  // validation.  Check for unused abbrevs should be skipped.
  std::vector< ::Dwarf_Off> _m_abbr_skip;

  // The check pass adds all low_pc/high_pc ranges loaded from DIE
  // tree into this coverage structure.
  coverage _m_cov;

  // If, during the check, we find any rangeptr-class attributes, we
  // set need_ranges to true.  cu_ranges pass then uses this as a hint
  // whether to request .debug_ranges or not.
  bool _m_need_ranges;

  bool check_cu_structural (struct read_ctx *ctx,
			    struct cu *const cu,
			    Elf_Data *strings,
			    struct coverage *strings_coverage,
			    struct relocation_data *reloc);

public:
  static checkdescriptor const *descriptor ();

  coverage const &cov () const { return _m_cov; }
  bool need_ranges () const { return _m_need_ranges; }

  // This is where the loaded CUs are stored.
  std::vector<cu> cus;

  check_debug_info (checkstack &stack, dwarflint &lint);
  ~check_debug_info ();

  cu *find_cu (::Dwarf_Off offset);
};

/** Check pending references that need other sections to be validated
    first.  */
class check_debug_info_refs
  : public check<check_debug_info_refs>
{
  check_debug_info *_m_info;
  check_debug_line *_m_line;
  check_debug_aranges *_m_aranges;

public:
  static checkdescriptor const *descriptor ();
  check_debug_info_refs (checkstack &stack, dwarflint &lint);
};

#endif//DWARFLINT_CHECK_DEBUG_INFO_HH
