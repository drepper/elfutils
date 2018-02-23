/* Known named integer values in DWARF.
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

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
#define DWARF_ONE_KNOWN_DW_TAG(name, id)		case id: return #id;
#define DWARF_ONE_KNOWN_DW_TAG_DESC(name, id, desc)	DWARF_ONE_KNOWN_DW_TAG (name, id)
      DWARF_ALL_KNOWN_DW_TAG
    }
  return NULL;
}

const char *
dwarf::known_attribute (int name)
{
  switch (name)
    {
#define DWARF_ONE_KNOWN_DW_AT(name, id)		case id: return #id;
#define DWARF_ONE_KNOWN_DW_AT_DESC(name, id, desc)	DWARF_ONE_KNOWN_DW_AT (name, id)
      DWARF_ALL_KNOWN_DW_AT
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

#define DWARF_ALL_KNOWN_ENUM				\
  DWARF_KNOWN_ENUM (accessibility, ACCESS)		\
  DWARF_KNOWN_ENUM (encoding, ATE)			\
  DWARF_KNOWN_ENUM (calling_convention, CC)		\
  DWARF_KNOWN_ENUM (decimal_sign, DS)			\
  DWARF_KNOWN_ENUM (endianity, END)			\
  DWARF_KNOWN_ENUM (identifier_case, ID)		\
  DWARF_KNOWN_ENUM (inline, INL)			\
  DWARF_KNOWN_ENUM (language, LANG)			\
  DWARF_KNOWN_ENUM (ordering, ORD)			\
  DWARF_KNOWN_ENUM (virtuality, VIRTUALITY)		\
  DWARF_KNOWN_ENUM (visibility, VIS)

#define DWARF_ONE_KNOWN_DW_ACCESS(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_ATE(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_CC(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_DS(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_END(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_ID(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_INL(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_LANG(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_LANG_DESC(name, id, desc)	DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_ORD(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_INL(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_VIRTUALITY(name, id)	DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_VIS(name, id)		DWARF_KNOWN_ENUM_CASE (id)

  // Stupid C++ doesn't do [x] = y initializers.
#define DWARF_KNOWN_ENUM(attr, enum)                   \
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
	DWARF_ALL_KNOWN_DW_##enum				\
      }							\
    return NULL;					\
  }
#define DWARF_KNOWN_ENUM_CASE(id)	case id: return #id;

  DWARF_ALL_KNOWN_ENUM

  // Not really enum cases, but pretend they are.
#define DWARF_ONE_KNOWN_DW_FORM(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_OP(name, id)		DWARF_KNOWN_ENUM_CASE (id)
#define DWARF_ONE_KNOWN_DW_OP_DESC(name, id, desc)	DWARF_KNOWN_ENUM_CASE (id)
  DWARF_KNOWN_ENUM (producer, FORM)
  DWARF_KNOWN_ENUM (location, OP)

#undef DWARF_KNOWN_ENUM
#undef DWARF_KNOWN_ENUM_CASE
};

static const char *
known_identifier (unsigned int which, unsigned int value)
{
  switch (which)
    {
# define DWARF_KNOWN_ENUM(attr, enum)						\
      case DW_AT_##attr:						\
	return dwarf::known_enum<DW_AT_##attr>::identifier (value);

      DWARF_ALL_KNOWN_ENUM

# undef DWARF_KNOWN_ENUM
    }

  return NULL;
}

static const char *
known_name (unsigned int which, unsigned int value)
{
  switch (which)
    {
# define DWARF_KNOWN_ENUM(attr, enum)						\
      case DW_AT_##attr:						\
	return dwarf::known_enum<DW_AT_##attr>::name (value);

      DWARF_ALL_KNOWN_ENUM

# undef DWARF_KNOWN_ENUM
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
dwarf_data::dwarf_enum::identifier () const
{
  return enum_identifier (*this);
}

const char *
dwarf_data::dwarf_enum::name () const
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
elfutils::to_string<dwarf::dwarf_enum> (const dwarf::dwarf_enum &value)
{
  return enum_string (value);
}

template<>
string
elfutils::to_string<dwarf_data::dwarf_enum> (const dwarf_data::dwarf_enum &value)
{
  return enum_string (value);
}
