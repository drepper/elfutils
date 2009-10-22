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

class check_debug_ranges
  : public check<check_debug_ranges>
{
  section<sec_ranges> *_m_sec_ranges;
  check_debug_info *_m_cus;

public:
  explicit check_debug_ranges (dwarflint &lint);
};
static reg<check_debug_ranges> reg_debug_ranges;

class check_debug_aranges
  : public check<check_debug_aranges>
{
  section<sec_aranges> *_m_sec_aranges;

public:
  explicit check_debug_aranges (dwarflint &lint);
};
static reg<check_debug_aranges> reg_debug_aranges;

class check_debug_loc
  : public check<check_debug_loc>
{
  section<sec_loc> *_m_sec_loc;
  check_debug_info *_m_cus;

public:
  explicit check_debug_loc (dwarflint &lint);
};
static reg<check_debug_loc> reg_debug_loc;
