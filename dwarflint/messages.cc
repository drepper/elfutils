/* Pedantic checking of DWARF files
   Copyright (C) 2009-2011 Red Hat, Inc.
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

#include "messages.hh"
#include "coverage.hh"
#include "option.hh"

#include <vector>
#include <sstream>
#include <cassert>
#include <cstdarg>
#include <libintl.h>

unsigned error_count = 0;
bool message_context::_m_last_emitted;

bool
message_accept (struct message_criteria const *cri,
		unsigned long cat)
{
  for (size_t i = 0; i < cri->size (); ++i)
    {
      message_term const &t = cri->at (i);
      if ((t.positive & cat) == t.positive
	  && (t.negative & cat) == 0)
	return true;
    }

  return false;
}

namespace
{
  struct cat_to_str
    : public std::vector<std::string>
  {
    cat_to_str ()
    {
      int count = 0;
#define MC(CAT, ID) if (ID > count) count = ID;
      MESSAGE_CATEGORIES
#undef MC

      resize (count + 1);
#define MC(CAT, ID) (*this)[ID] = #CAT;
      MESSAGE_CATEGORIES
#undef MC
    }
  } cat_names;
  size_t cat_max = cat_names.size ();
}


message_category
operator | (message_category a, message_category b)
{
  return static_cast<message_category> ((unsigned long)a | b);
}

message_category &
operator |= (message_category &a, message_category b)
{
  a = a | b;
  return a;
}

std::string
message_term::str () const
{
  std::ostringstream os;
  os << '(';

  bool got = false;
  for (size_t i = 0; i <= cat_max; ++i)
    {
      size_t mask = 1u << i;
      if ((positive & mask) != 0
	  || (negative & mask) != 0)
	{
	  if (got)
	    os << " & ";
	  if ((negative & (1u << i)) != 0)
	    os << '~';
	  os << cat_names[i];
	  got = true;
	}
    }

  if (!got)
    os << '1';

  os << ')';
  return os.str ();
}

std::string
message_criteria::str () const
{
  std::ostringstream os;

  for (size_t i = 0; i < size (); ++i)
    {
      message_term const &t = at (i);
      if (i > 0)
	os << " | ";
      os << t.str ();
    }

  return os.str ();
}

void
message_criteria::operator &= (message_term const &term)
{
  assert ((term.positive & term.negative) == 0);
  for (size_t i = 0; i < size (); )
    {
      message_term &t = at (i);
      t.positive = t.positive | term.positive;
      t.negative = t.negative | term.negative;
      if ((t.positive & t.negative) != 0)
	/* A ^ ~A -> drop the term.  */
	erase (begin () + i);
      else
	++i;
    }
}

void
message_criteria::operator |= (message_term const &term)
{
  assert ((term.positive & term.negative) == 0);
  push_back (term);
}

// xxx this one is inaccessible from the outside.  Make it like &=, |=
// above
/* NEG(a&b&~c) -> (~a + ~b + c) */
message_criteria
operator ! (message_term const &term)
{
  assert ((term.positive & term.negative) == 0);

  message_criteria ret;
  for (size_t i = 0; i < cat_max; ++i)
    {
      unsigned mask = 1u << i;
      if ((term.positive & mask) != 0)
	ret |= message_term ((message_category)(1u << i), mc_none);
      else if ((term.negative & mask) != 0)
	ret |= message_term (mc_none, (message_category)(1u << i));
    }

  return ret;
}

std::ostream &
operator<< (std::ostream &o, message_category cat)
{
  o << '(';

  bool got = false;
  for (size_t i = 0; i <= cat_max; ++i)
    {
      size_t mask = 1u << i;
      if ((cat & mask) != 0)
	{
	  if (got)
	    o << ",";
	  o << cat_names[i];
	  got = true;
	}
    }

  if (!got)
    o << "none";

  return o << ')';
}

std::ostream &
operator<< (std::ostream &o, message_term const &term)
{
  return o << term.str ();
}

std::ostream &
operator<< (std::ostream &o, __attribute__ ((unused)) message_criteria const &criteria)
{
  return o << criteria.str ();
}

/* MUL((a&b + c&d), (e&f + g&h)) -> (a&b&e&f + a&b&g&h + c&d&e&f + c&d&g&h) */
void
message_criteria::operator *= (message_criteria const &rhs)
{
  message_criteria ret;
  for (size_t i = 0; i < size (); ++i)
    for (size_t j = 0; j < rhs.size (); ++j)
      {
	message_term t1 = at (i);
	message_term const &t2 = rhs.at (j);
	t1.positive |= t2.positive;
	t1.negative |= t2.negative;
	if (t1.positive & t1.negative)
	  /* A ^ ~A -> drop the term.  */
	  continue;
	ret |= t1;
      }

  *this = ret;
}

// xxx this one is inaccessible from the outside.  Bind it properly
/* Reject message if TERM passes.  */
void
message_criteria::and_not (message_term const &term)
{
  // xxxxx really??  "!"??
  message_criteria tmp = !message_term (term.negative, term.positive);
  *this *= tmp;
}

static void
wr_verror (locus const &loc, const char *format, va_list ap)
{
  printf ("error: %s", loc.format ().c_str ());
  vprintf (format, ap);
  ++error_count;
}

static void
wr_vwarning (locus const &loc, const char *format, va_list ap)
{
  printf ("%s", loc.format ().c_str ());
  vprintf (format, ap);
  ++error_count;
}

void
wr_error (locus const *loc, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  wr_verror (*loc, format, ap);
  va_end (ap);
}

void
wr_message (unsigned long category, locus const *loc,
	    const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  // Clumsy duplicate filtering. Use format as key.
  bool whether = false;
  message_category cat = (message_category) category;
  wr_message (cat).id (format, whether);
  if (whether && message_accept (&warning_criteria, category))
    {
      if (message_accept (&error_criteria, category))
	wr_verror (*loc, format, ap);
      else
	wr_vwarning (*loc, format, ap);
    }
  va_end (ap);
}

namespace
{
  class nostream: public std::ostream {};
  static nostream nostream;

  std::ostream &
  get_stream ()
  {
    return std::cout;
  }
}

global_opt<unsigned_option>
  dup_threshold_opt ("Threshold for duplicate messages."
		     " Defaults to 16."
		     " Use zero for no limit.",
		     "count", "dups");

namespace
{
  unsigned
  dup_threshold ()
  {
    static unsigned t = dup_threshold_opt.value (16);
    if (t == 0)
      t = -1;
    return t;
  }
}

int
message_count_filter::should_emit (void const *key)
{
  unsigned count = ++_m_counters[key];
  if (count > dup_threshold ())
    return 0;
  else if (count == dup_threshold ())
    return -1;
  else
    return 1;
}

message_context::message_context (message_count_filter *filter,
				  locus const *loc, char const *prefix)
  : _m_filter (filter)
  , _m_loc (loc)
  , _m_prefix (prefix)
{}

std::ostream &
message_context::when (bool whether) const
{
  _m_last_emitted = false;
  if (whether)
    {
      ++error_count;
      _m_last_emitted = true;

      std::ostream &ret = get_stream ();
      ret << _m_prefix;
      if (_m_loc != NULL)
	ret << _m_loc->format () << ": ";
      return ret;
    }
  else
    return nostream;
}

std::ostream &
message_context::when_prev () const
{
  return when (wr_prev_emitted ());
}

std::ostream &
message_context::id (void const *key, bool &whether)
{
  if (_m_filter == NULL)
    return nostream;
  else if (int status = _m_filter->should_emit (key))
    {
      if (status == -1)
	get_stream () << "(threshold [--dups=" << dup_threshold ()
		      << "] reached for the following message)"
		      << std::endl;
      whether = true;
      return when (true);
    }
  else
    return nostream;
}

std::ostream &
message_context::id (void const *key)
{
  bool whether;
  return id (key, whether);
}

std::ostream &
message_context::operator << (char const *message)
{
  return id (message) << message;
}

std::ostream &
message_context::operator << (std::string const &message)
{
  return *this << message.c_str ();
}

std::ostream &
wr_error ()
{
  ++error_count;
  return get_stream () << gettext ("error: ");
}

std::ostream &
wr_error (locus const &loc)
{
  std::string fmt = loc.format ();
  return wr_error () << fmt << ": ";
}

message_context
message_context::filter_message (locus const *loc, message_category category)
{
  if (!message_accept (&warning_criteria, category))
    return message_context (NULL, NULL, NULL);
  else if (message_accept (&error_criteria, category))
    return message_context (message_count_filter::inst (),
			    loc, "error: ");
  else
    return message_context (message_count_filter::inst (),
			    loc, "warning: ");
}

message_context
wr_message (message_category category)
{
  return message_context::filter_message (NULL, category);
}

message_context
wr_message (locus const &loc, message_category category)
{
  return message_context::filter_message (&loc, category);
}

void
wr_format_padding_message (message_category category,
			   locus const &loc,
			   uint64_t start, uint64_t end, char const *kind)
{
  char msg[128];
  wr_message (loc, category)
    << range_fmt (msg, sizeof msg, start, end)
    << ": " << kind << "." << std::endl;
}

void
wr_format_leb128_message (locus const &loc,
			  const char *what,
			  const char *purpose,
			  const unsigned char *begin, const unsigned char *end)
{
  message_category category = mc_leb128 | mc_acc_bloat | mc_impact_3;
  char buf[(end - begin) * 3 + 1]; // 2 hexa digits+" " per byte, and term. 0
  char *ptr = buf;
  for (; begin < end; ++begin)
    ptr += sprintf (ptr, " %02x", *begin);
  wr_message (loc, category)
    << what << ": value " << purpose << " encoded as `"
    << (buf + 1) << "'." << std::endl;
}

void
wr_message_padding_0 (message_category category,
		      locus const &loc,
		      uint64_t start, uint64_t end)
{
  wr_format_padding_message (category | mc_acc_bloat | mc_impact_1,
			     loc, start, end,
			     "unnecessary padding with zero bytes");
}

void
wr_message_padding_n0 (message_category category,
		       locus const &loc,
		       uint64_t start, uint64_t end)
{
  wr_format_padding_message (category | mc_acc_bloat | mc_impact_1,
			     loc, start, end,
			     "unreferenced non-zero bytes");
}

void
wr_reset_counters ()
{
  error_count = 0;
  message_count_filter::inst ()->clear ();
}

bool
wr_prev_emitted ()
{
  return message_context::_m_last_emitted;
}
