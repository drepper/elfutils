#include "dwarflint-checks.hh"
#include "dwarflint-low.h"

class load_sections
  : public check<load_sections>
{
public:
  elf_file file;
  explicit load_sections (dwarflint &lint);
};

class section_base
{
  load_sections *sections;
  sec &get_sec_or_throw (section_id secid);
public:
  sec &sect;
  elf_file &file;
  section_base (dwarflint &lint, section_id secid);
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

class check_debug_abbrev
  : public check<check_debug_abbrev>
{
  section<sec_abbrev> *_m_sec_abbr;

public:
  explicit check_debug_abbrev (dwarflint &lint);

  // offset -> abbreviations
  std::map < ::Dwarf_Off, abbrev_table> abbrevs;
  struct abbrev_table *abbrev_chain; // xxx
};
static reg <check_debug_abbrev> reg_debug_abbrev;

class check_debug_info
  : public check<check_debug_info>
{
  section<sec_info> *_m_sec_info;
  section<sec_abbrev> *_m_sec_abbrev;
  section<sec_str> *_m_sec_str;
  check_debug_abbrev *_m_abbrevs;

public:
  cu_coverage cu_cov;
  std::vector <cu> cus;
  cu *cu_chain; // xxx

  explicit check_debug_info (dwarflint &lint);
};
static reg <check_debug_info> reg_debug_info;
