/* Known named integer values in DWARF.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include <config.h>
#include "dwarf"
#include "dwarf_edit"
#include "known-dwarf.h"

using namespace elfutils;
using namespace std;


const char *
dwarf::known_tag (int tag)
{
  switch (tag)
    {
#define ONE_KNOWN_DW_TAG(name, id)		case id: return #id;
#define ONE_KNOWN_DW_TAG_DESC(name, id, desc)	ONE_KNOWN_DW_TAG (name, id)
      ALL_KNOWN_DW_TAG
    }
  return NULL;
}

const char *
dwarf::known_attribute (int name)
{
  switch (name)
    {
#define ONE_KNOWN_DW_AT(name, id)		case id: return #id;
#define ONE_KNOWN_DW_AT_DESC(name, id, desc)	ONE_KNOWN_DW_AT (name, id)
      ALL_KNOWN_DW_AT
    }
  return NULL;
}

namespace elfutils
{
  template<int key>
  size_t
  dwarf::known_enum<key>::prefix_length ()
  {
    return 0;
  }

  template<int key>
  const char *
  dwarf::known_enum<key>::identifier (int value)
  {
    return NULL;
  }

#define ALL_KNOWN_ENUM				\
  KNOWN_ENUM (accessibility, ACCESS)		\
  KNOWN_ENUM (encoding, ATE)			\
  KNOWN_ENUM (calling_convention, CC)		\
  KNOWN_ENUM (decimal_sign, DS)			\
  KNOWN_ENUM (endianity, END)			\
  KNOWN_ENUM (identifier_case, ID)		\
  KNOWN_ENUM (inline, INL)			\
  KNOWN_ENUM (language, LANG)			\
  KNOWN_ENUM (ordering, ORD)			\
  KNOWN_ENUM (virtuality, VIRTUALITY)		\
  KNOWN_ENUM (visibility, VIS)

#define ONE_KNOWN_DW_ACCESS(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_ATE(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_CC(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_DS(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_END(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_ID(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_INL(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_LANG(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_LANG_DESC(name, id, desc)	KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_ORD(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_INL(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_VIRTUALITY(name, id)	KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_VIS(name, id)		KNOWN_ENUM_CASE (id)

  // Stupid C++ doesn't do [x] = y initializers.
#define KNOWN_ENUM(attr, enum)				\
  template<>						\
  size_t						\
  dwarf::known_enum<DW_AT_##attr>::prefix_length ()	\
  {						 	\
    return sizeof ("DW_" #enum "_") - 1;		\
  }							\
  template<>						\
  const char *						\
  dwarf::known_enum<DW_AT_##attr>::identifier (int value)	\
  {							\
    switch (value)					\
      {							\
	ALL_KNOWN_DW_##enum				\
      }							\
    return NULL;					\
  }
#define KNOWN_ENUM_CASE(id)	case id: return #id;

  ALL_KNOWN_ENUM

  // Not really enum cases, but pretend they are.
#define ONE_KNOWN_DW_FORM(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_OP(name, id)		KNOWN_ENUM_CASE (id)
#define ONE_KNOWN_DW_OP_DESC(name, id, desc)	KNOWN_ENUM_CASE (id)
  KNOWN_ENUM (producer, FORM)
  KNOWN_ENUM (location, OP)

#undef KNOWN_ENUM
#undef KNOWN_ENUM_CASE
};

static const char *
known_identifier (unsigned int which, unsigned int value)
{
  switch (which)
    {
# define KNOWN_ENUM(attr, enum)						\
      case DW_AT_##attr:						\
	return dwarf::known_enum<DW_AT_##attr>::identifier (value);

      ALL_KNOWN_ENUM

# undef KNOWN_ENUM
    }

  return NULL;
}

static const char *
known_name (unsigned int which, unsigned int value)
{
  switch (which)
    {
# define KNOWN_ENUM(attr, enum)						\
      case DW_AT_##attr:						\
	return dwarf::known_enum<DW_AT_##attr>::name (value);

      ALL_KNOWN_ENUM

# undef KNOWN_ENUM
    }

  return NULL;
}

template<typename constant>
static inline const char *
enum_identifier (const constant &value)
{
  return known_identifier (value.which (), value);
}

template<typename constant>
static inline const char *
enum_name (const constant &value)
{
  return known_name (value.which (), value);
}

const char *
dwarf::dwarf_enum::identifier () const
{
  return enum_identifier (*this);
}

const char *
dwarf::dwarf_enum::name () const
{
  return enum_name (*this);
}

const char *
dwarf_edit::dwarf_enum::identifier () const
{
  return enum_identifier (*this);
}

const char *
dwarf_edit::dwarf_enum::name () const
{
  return enum_name (*this);
}

template<class value_type>
static inline std::string
enum_string (const value_type &value)
{
  const char *known = value.name ();
  return known == NULL ? subr::hex_string (value) : std::string (known);
}

template<>
string
to_string<dwarf::dwarf_enum> (const dwarf::dwarf_enum &value)
{
  return enum_string (value);
}

template<>
string
to_string<dwarf_edit::dwarf_enum> (const dwarf_edit::dwarf_enum &value)
{
  return enum_string (value);
}
