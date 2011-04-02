/* Copyright (C) 1999-2011 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

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

#ifndef DWARFSTRINGS_H
#define DWARFSTRINGS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

const char *dwarf_tag_string (unsigned int tag);

const char *dwarf_attr_string (unsigned int attrnum);

const char *dwarf_form_string (unsigned int form);

const char *dwarf_lang_string (unsigned int lang);

const char *dwarf_inline_string (unsigned int code);

const char *dwarf_encoding_string (unsigned int code);

const char *dwarf_access_string (unsigned int code);

const char *dwarf_visibility_string (unsigned int code);

const char *dwarf_virtuality_string (unsigned int code);

const char *dwarf_identifier_case_string (unsigned int code);

const char *dwarf_calling_convention_string (unsigned int code);

const char *dwarf_ordering_string (unsigned int code);

const char *dwarf_discr_list_string (unsigned int code);

const char *dwarf_locexpr_opcode_string (unsigned int code);

const char *dwarf_line_standard_opcode_string (unsigned int code);

const char *dwarf_line_extended_opcode_string (unsigned int code);

#ifdef __cplusplus
}
#endif

#endif  /* dwarfstrings.h */
