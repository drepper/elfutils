/*
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

#ifndef DWARFLINT_CHECKS_LOW_HH
#define DWARFLINT_CHECKS_LOW_HH

#include "low.h"
#include "checks.hh"

class load_sections
  : public check<load_sections>
{
public:
  elf_file file;
  explicit load_sections (dwarflint &lint);
  ~load_sections ();
};

class section_base
{
  load_sections *sections;
  sec &get_sec_or_throw (section_id secid);
public:
  sec &sect;
  elf_file &file;
  section_base (dwarflint &lint, section_id secid);

  relocation_data *reldata () const
  {
    return sect.rel.size > 0 ? &sect.rel : NULL;
  }
};

template<section_id sec_id>
class section
  : public section_base
  , public check<section<sec_id> >
{
public:
  explicit section (dwarflint &lint)
    : section_base (lint, sec_id)
  {}
};

/** The pass for reading basic .debug_info data -- the layout of
    sections and their headers.  */
class read_cu_headers
  : public check<read_cu_headers>
{
  section<sec_info> *_m_sec_info;

public:
  std::vector<cu_head> const cu_headers;
  explicit read_cu_headers (dwarflint &lint);
};

class check_debug_abbrev
  : public check<check_debug_abbrev>
{
  section<sec_abbrev> *_m_sec_abbr;

  bool check_no_abbreviations () const;

public:
  explicit check_debug_abbrev (dwarflint &lint);

  // offset -> abbreviations
  typedef std::map< ::Dwarf_Off, abbrev_table> abbrev_map;
  abbrev_map abbrevs;
};
static reg<check_debug_abbrev> reg_debug_abbrev;

class check_debug_info
  : public check<check_debug_info>
{
  section<sec_info> *_m_sec_info;
  section<sec_abbrev> *_m_sec_abbrev;
  section<sec_str> *_m_sec_str;
  check_debug_abbrev *_m_abbrevs;
  read_cu_headers *_m_cu_headers;

public:
  // The check pass adds all low_pc/high_pc ranges loaded from DIE
  // tree into this following cu_cov structure.  If it finds any
  // rangeptr-class attributes, it sets cu_cov.need_ranges to true.
  cu_coverage cu_cov;
  std::vector<cu> cus;

  explicit check_debug_info (dwarflint &lint);
  ~check_debug_info ();
};
static reg<check_debug_info> reg_debug_info;

class check_debug_aranges
  : public check<check_debug_aranges>
{
  section<sec_aranges> *_m_sec_aranges;

public:
  explicit check_debug_aranges (dwarflint &lint);
};
static reg<check_debug_aranges> reg_debug_aranges;

#endif//DWARFLINT_CHECKS_LOW_HH
