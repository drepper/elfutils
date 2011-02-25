/*
   Copyright (C) 2010, 2011 Red Hat, Inc.
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

#include "highlevel_check.hh"
#include "all-dies-it.hh"
#include "option.hh"
#include "messages.hh"
#include "pri.hh"

#include <sstream>
#include <bitset>

using elfutils::dwarf;

namespace
{

#define DIE_OPTSTRING					\
  "}[,...]"

  string_option opt_ignore
  ("Skip certain DIEs.  class may be one of single_addr, artificial, inlined, \
inlined_subroutine, no_coverage, mutable, or immutable.",
   "class[,...]", "locstats:ignore");

  string_option opt_dump
  ("Dump certain DIEs.  For classes, see option 'ignore'.",
   "class[,...]", "locstats:dump");

  string_option opt_tabulation_rule
  ("Rule for sorting results into buckets. start is either integer 0..100, \
or special value 0.0 indicating cases with no coverage whatsoever \
(i.e. not those that happen to round to 0%).",
   "start[:step][,...]", "locstats:tabulate");

  class locstats
    : public highlevel_check<locstats>
  {
  public:
    static checkdescriptor const *descriptor () {
      static checkdescriptor cd
	(checkdescriptor::create ("locstats")
	 .groups ("@nodefault")
	 .option (opt_ignore)
	 .option (opt_dump)
	 .option (opt_tabulation_rule)
	 .description (
"Computes a location info coverage statistics.  Goes through the whole "
"DIE graph, looking at each variable and formal parameter, and "
"determining scope coverage of its location information.  In other "
"words for how big a part of scope we know, where the variable "
"\"lives\".\n"
" - https://fedorahosted.org/pipermail/elfutils-devel/2010-July/001498.html\n"
" - https://fedorahosted.org/pipermail/elfutils-devel/2010-September/001602.html\n"));
      return &cd;
    }

    locstats (checkstack &stack, dwarflint &lint);
  };

  reg<locstats> reg_locstats;

#define DIE_TYPES		\
  TYPE(single_addr)		\
  TYPE(artificial)		\
  TYPE(inlined)			\
  TYPE(inlined_subroutine)	\
  TYPE(no_coverage)		\
  TYPE(mutable)			\
  TYPE(immutable)

  struct tabrule
  {
    int start;
    int step;
    tabrule (int a_start, int a_step)
      : start (a_start), step (a_step)
    {}
    bool operator < (tabrule const &other) const {
      return start < other.start;
    }
  };

  // Sharp 0.0% coverage (i.e. not a single address byte is covered)
  const int cov_00 = -1;

  struct tabrules_t
    : public std::vector<tabrule>
  {
    tabrules_t (std::string const &rule)
    {
      std::stringstream ss;
      ss << rule;

      std::string item;
      while (std::getline (ss, item, ','))
	{
	  if (item.empty ())
	    continue;
	  int start;
	  int step;
	  char const *ptr = item.c_str ();

	  if (item.length () >= 3
	      && std::strncmp (ptr, "0.0", 3) == 0)
	    {
	      start = cov_00;
	      ptr += 3;
	    }
	  else
	    start = std::strtol (ptr, const_cast<char **> (&ptr), 10);

	  if (*ptr == 0)
	    step = 0;
	  else
	    {
	      if (*ptr != ':')
		{
		  step = 0;
		  goto garbage;
		}
	      else
		ptr++;

	      step = std::strtol (ptr, const_cast<char **> (&ptr), 10);
	      if (*ptr != 0)
	      garbage:
		std::cerr << "Ignoring garbage at the end of the rule item: '"
			  << ptr << '\'' << std::endl;
	    }

	  push_back (tabrule (start, step));
	}

      push_back (tabrule (100, 0));
      std::sort (begin (), end ());
    }

    void next ()
    {
      if (at (0).step == 0)
	erase (begin ());
      else
	{
	  if (at (0).start == cov_00)
	    at (0).start = 0;
	  at (0).start += at (0).step;
	  if (size () > 1)
	    {
	      if (at (0).start > at (1).start)
		erase (begin ());
	      while (size () > 1
		     && at (0).start == at (1).start)
		erase (begin ());
	    }
	}
    }

    bool match (int value) const
    {
      return at (0).start == value;
    }
  };

#define TYPE(T) dt_##T,
  enum die_type_e
    {
      DIE_TYPES
      dt__count
    };
#undef TYPE

  class die_type_matcher
    : public std::bitset<dt__count>
  {
    class invalid {};
    std::pair<die_type_e, bool>
    parse (std::string &desc)
    {
      bool val = true;
      if (desc == "")
	throw invalid ();

#define TYPE(T)					\
      if (desc == #T)				\
	return std::make_pair (dt_##T, val);
      DIE_TYPES
#undef TYPE

      throw invalid ();
    }

  public:
    die_type_matcher (std::string const &rule)
    {
      std::stringstream ss;
      ss << rule;

      std::string item;
      while (std::getline (ss, item, ','))
	try
	  {
	    std::pair<die_type_e, bool> const &ig = parse (item);
	    set (ig.first, ig.second);
	  }
	catch (invalid &i)
	  {
	    std::cerr << "Invalid die type: " << item << std::endl;
	  }
    }
  };

  class mutability_t
  {
    bool _m_is_mutable;
    bool _m_is_immutable;

  public:
    mutability_t ()
      : _m_is_mutable (false)
      , _m_is_immutable (false)
    {
    }

    void set (bool what)
    {
      if (what)
	_m_is_mutable = true;
      else
	_m_is_immutable = true;
    }

    void set_both ()
    {
      set (true);
      set (false);
    }

    void locexpr (Dwarf_Op *expr, size_t len)
    {
      // We scan the expression looking for DW_OP_{bit_,}piece
      // operators which mark ends of sub-expressions to us.
      bool m = false;
      for (size_t i = 0; i < len; ++i)
	switch (expr[i].atom)
	  {
	  case DW_OP_implicit_value:
	  case DW_OP_stack_value:
	    m = true;
	    break;

	  case DW_OP_bit_piece:
	  case DW_OP_piece:
	    set (m);
	    m = false;
	    break;
	  };
      set (m);
    }

    bool is_mutable () const { return _m_is_mutable; }
    bool is_immutable () const { return _m_is_immutable; }
  };

  struct error
    : public std::runtime_error
  {
    explicit error (std::string const &what_arg)
      : std::runtime_error (what_arg)
    {}
  };

  // Look through the stack of parental dies and return the non-empty
  // ranges instance closest to the stack top (i.e. die_stack.end ()).
  dwarf::ranges
  find_ranges (std::vector<dwarf::debug_info_entry> const &die_stack)
  {
    for (auto it = die_stack.rbegin (); it != die_stack.rend (); ++it)
      if (!it->ranges ().empty ())
	return it->ranges ();
    throw error ("no ranges for this DIE");
  }

  bool
  is_inlined (dwarf::debug_info_entry const &die)
  {
    dwarf::debug_info_entry::attributes_type::const_iterator it
      = die.attributes ().find (DW_AT_inline);
    if (it != die.attributes ().end ())
      {
	char const *name = (*it).second.dwarf_constant ().name ();
	return std::strcmp (name, "declared_inlined") == 0
	  || std::strcmp (name, "inlined") == 0;
      }
    return false;
  }
}

locstats::locstats (checkstack &stack, dwarflint &lint)
  : highlevel_check<locstats> (stack, lint)
{
  // map percentage->occurrences.  Percentage is cov_00..100, where
  // 0..100 is rounded-down integer division.
  std::map<int, unsigned long> tally;
  unsigned long total = 0;
  for (int i = 0; i <= 100; ++i)
    tally[i] = 0;

  tabrules_t tabrules (/*opt_tabulation_rule.seen ()
			 ? opt_tabulation_rule.value () :*/ "10:10");
  die_type_matcher ignore (/*opt_ignore.seen () ? opt_ignore.value () :*/ "");
  die_type_matcher dump (/*opt_dump.seen () ? opt_dump.value () :*/ "");
  std::bitset<dt__count> interested = ignore | dump;
  bool interested_mutability
    = interested.test (dt_mutable) || interested.test (dt_immutable);


  for (all_dies_iterator<dwarf> it = all_dies_iterator<dwarf> (dw);
       it != all_dies_iterator<dwarf> (); ++it)
    {
      std::bitset<dt__count> die_type;
      dwarf::debug_info_entry const &die = *it;

      // We are interested in variables and formal parameters
      bool is_formal_parameter = die.tag () == DW_TAG_formal_parameter;
      if (!is_formal_parameter && die.tag () != DW_TAG_variable)
	continue;

      dwarf::debug_info_entry::attributes_type const &attrs
	= die.attributes ();

      // ... except those that are just declarations
      if (attrs.find (DW_AT_declaration) != attrs.end ())
	continue;

      if (ignore.test (dt_artificial)
	  && attrs.find (DW_AT_artificial) != attrs.end ())
	continue;

      // Of formal parameters we ignore those that are children of
      // subprograms that are themselves declarations.
      std::vector<dwarf::debug_info_entry> const &die_stack = it.stack ();
      dwarf::debug_info_entry const &parent = *(die_stack.rbegin () + 1);
      if (is_formal_parameter)
	if (parent.tag () == DW_TAG_subroutine_type
	    || (parent.attributes ().find (DW_AT_declaration)
		!= parent.attributes ().end ()))
	  continue;

      if (interested.test (dt_inlined)
	  || interested.test (dt_inlined_subroutine))
	{
	  bool inlined = false;
	  bool inlined_subroutine = false;
	  for (std::vector<dwarf::debug_info_entry>::const_reverse_iterator
		 stit = die_stack.rbegin (); stit != die_stack.rend (); ++stit)
	    {
	      if (interested.test (dt_inlined)
		  && stit->tag () == DW_TAG_subprogram
		  && is_inlined (*stit))
		{
		  inlined = true;
		  if (interested.test (dt_inlined_subroutine)
		      && inlined_subroutine)
		    break;
		}
	      if (interested.test (dt_inlined_subroutine)
		  && stit->tag () == DW_TAG_inlined_subroutine)
		{
		  inlined_subroutine = true;
		  if (interested.test (dt_inlined)
		      && inlined)
		    break;
		}
	    }

	  if (inlined)
	    {
	      if (ignore.test (dt_inlined))
		continue;
	      die_type.set (dt_inlined);
	    }
	  if (inlined_subroutine)
	    {
	      if (ignore.test (dt_inlined_subroutine))
		continue;
	      die_type.set (dt_inlined_subroutine, inlined_subroutine);
	    }
	}

      // Unfortunately the location expression is not yet wrapped
      // in c++, so we need to revert back to C code.
      Dwarf_Die die_c_mem,
	*die_c = dwarf_offdie (this->c_dw, die.offset (), &die_c_mem);
      assert (die_c != NULL);

      Dwarf_Attribute locattr_mem,
	*locattr = dwarf_attr_integrate (die_c, DW_AT_location, &locattr_mem);

      // Also ignore extern globals -- these have DW_AT_external and
      // no DW_AT_location.
      if (attrs.find (DW_AT_external) != attrs.end () && locattr == NULL)
	continue;

      /*
      Dwarf_Attribute name_attr_mem,
	*name_attr = dwarf_attr_integrate (die_c, DW_AT_name, &name_attr_mem);
      std::string name = name_attr != NULL
	? dwarf_formstring (name_attr)
	: (dwarf_hasattr_integrate (die_c, DW_AT_artificial)
	   ? "<artificial>" : "???");

      std::cerr << "die=" << std::hex << die.offset ()
		<< " '" << name << '\'';
      */

      int coverage;
      Dwarf_Op *expr;
      size_t len;
      mutability_t mut;

      // consts need no location
      if (attrs.find (DW_AT_const_value) != attrs.end ())
	{
	  coverage = 100;
	  if (interested_mutability)
	    mut.set (true);
	}

      // no location
      else if (locattr == NULL)
	{
	  coverage = cov_00;
	  if (interested_mutability)
	    mut.set_both ();
	}

      // non-list location
      else if (dwarf_getlocation (locattr, &expr, &len) == 0)
	{
	  // Globals and statics have non-list location that is a
	  // singleton DW_OP_addr expression.
	  if (len == 1 && expr[0].atom == DW_OP_addr)
	    {
	      if (ignore.test (dt_single_addr))
		continue;
	      die_type.set (dt_single_addr);
	    }
	  if (interested_mutability)
	    mut.locexpr (expr, len);
	  coverage = (len == 0) ? cov_00 : 100;
	}

      // location list
      else
	{
	  try
	    {
	      dwarf::ranges ranges (find_ranges (die_stack));
	      size_t length = 0;
	      size_t covered = 0;

	      // Arbitrarily assume that there will be no more than 10
	      // expressions per address.
	      size_t nlocs = 10;
	      Dwarf_Op *exprs[nlocs];
	      size_t exprlens[nlocs];

	      for (dwarf::ranges::const_iterator rit = ranges.begin ();
		   rit != ranges.end (); ++rit)
		{
		  Dwarf_Addr low = (*rit).first;
		  Dwarf_Addr high = (*rit).second;
		  length += high - low;
		  //std::cerr << " " << low << ".." << high << std::endl;

		  for (Dwarf_Addr addr = low; addr < high; ++addr)
		    {
		      int got = dwarf_getlocation_addr (locattr, addr,
							exprs, exprlens, nlocs);
		      if (got < 0)
			throw ::error (std::string ("dwarf_getlocation_addr: ")
				       + dwarf_errmsg (-1));

		      // At least one expression for the address must
		      // be of non-zero length for us to count that
		      // address as covered.
		      for (int i = 0; i < got; ++i)
			{
			  if (interested_mutability)
			    mut.locexpr (exprs[i], exprlens[i]);
			  if (exprlens[i] > 0)
			    {
			      covered++;
			      break;
			    }
			}
		    }
		}

	      if (length == 0)
		throw ::error ("zero-length range");

	      if (covered == 0)
		coverage = cov_00;
	      else
		coverage = 100 * covered / length;
	    }
	  catch (::error const &e)
	    {
	      struct where where = WHERE (sec_info, NULL);
	      where_reset_1 (&where, it.cu ().offset ());
	      where_reset_2 (&where, die.offset ());
	      wr_error (where) << e.what () << '.' << std::endl;
	      continue;
	    }
	}

      if (coverage == cov_00)
	{
	  if (ignore.test (dt_no_coverage))
	    continue;
	  die_type.set (dt_no_coverage);
	}
      else if (interested_mutability)
	{
	  assert (mut.is_mutable () || mut.is_immutable ());
	  if (mut.is_mutable ())
	    {
	      if (ignore.test (dt_mutable))
		continue;
	      die_type.set (dt_mutable);
	    }
	  if (mut.is_immutable ())
	    {
	      if (ignore.test (dt_immutable))
		continue;
	      die_type.set (dt_immutable);
	    }
	}

      if ((dump & die_type).any ())
	{
#define TYPE(T) << (die_type.test (dt_##T) ? " "#T : "")
	  std::cerr << "dumping" DIE_TYPES << " DIE" << std::endl;
#undef TYPE

	  std::string pad = "";
	  for (auto sit = die_stack.begin (); sit != die_stack.end (); ++sit)
	    {
	      auto const &d = *sit;
	      std::cerr << pad << pri::ref (d) << " "
			<< elfutils::dwarf::tags::name (d.tag ()) << std::endl;
	      for (auto atit = d.attributes ().begin ();
		   atit != d.attributes ().end (); ++atit)
		{
		  auto const &attr = *atit;
		  std::cerr << pad << "    " << to_string (attr) << std::endl;
		}
	      pad += " ";
	    }

	  std::cerr << "empty coverage " << pri::ref (die) << " "
		    << to_string (die) << std::endl;
	}

      tally[coverage]++;
      total++;
      //std::cerr << std::endl;
    }

  unsigned long cumulative = 0;
  unsigned long last = 0;
  int last_pct = cov_00;
  std::cout << "cov%\tsamples\tcumul" << std::endl;
  for (int i = cov_00; i <= 100; ++i)
    {
      cumulative += tally.find (i)->second;
      if (tabrules.match (i))
	{
	  long int samples = cumulative - last;

	  // The case 0.0..x should be printed simply as 0
	  if (last_pct == cov_00 && i > cov_00)
	    last_pct = 0;

	  if (last_pct == cov_00)
	    std::cout << "0.0";
	  else
	    std::cout << std::dec << last_pct;

	  if (last_pct != i)
	    std::cout << ".." << i;
	  std::cout << "\t" << samples
		    << '/' << (100*samples / total) << '%'
		    << "\t" << cumulative
		    << '/' << (100*cumulative / total) << '%'
		    << std::endl;
	  last = cumulative;
	  last_pct = i + 1;

	  tabrules.next ();
	}
    }
}
