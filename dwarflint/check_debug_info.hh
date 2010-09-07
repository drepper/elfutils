/* Low-level checking of .debug_info.
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

#ifndef DWARFLINT_CHECK_DEBUG_INFO_HH
#define DWARFLINT_CHECK_DEBUG_INFO_HH

#include "low.h"
#include "checks.hh"
#include "check_debug_abbrev.ii"
#include "check_debug_line.ii"
#include "sections.ii"

/** The pass for reading basic .debug_info data -- the layout of
    sections and their headers.  */
class read_cu_headers
  : public check<read_cu_headers>
{
  section<sec_info> *_m_sec_info;

public:
  static checkdescriptor const &descriptor ();
  std::vector<cu_head> const cu_headers;
  read_cu_headers (checkstack &stack, dwarflint &lint);
};

/** The pass for in-depth structural analysis of .debug_info.  */
class check_debug_info
  : public check<check_debug_info>
{
  section<sec_info> *_m_sec_info;
  section<sec_abbrev> *_m_sec_abbrev;
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

  void check_info_structural ();

public:
  static checkdescriptor const &descriptor ();

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

public:
  static checkdescriptor const &descriptor ();
  check_debug_info_refs (checkstack &stack, dwarflint &lint);
};

#endif//DWARFLINT_CHECK_DEBUG_INFO_HH
