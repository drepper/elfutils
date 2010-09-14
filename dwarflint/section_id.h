/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

#ifndef DWARFLINT_SECTION_ID_H
#define DWARFLINT_SECTION_ID_H

#ifdef __cplusplus
extern "C"
{
#endif

#define DEBUGINFO_SECTIONS \
  SEC (info)		   \
  SEC (abbrev)		   \
  SEC (aranges)		   \
  SEC (pubnames)	   \
  SEC (pubtypes)	   \
  SEC (str)		   \
  SEC (line)		   \
  SEC (loc)		   \
  SEC (mac)		   \
  SEC (ranges)

  enum section_id
  {
    sec_invalid = 0,

    /* Debuginfo sections:  */
#define SEC(n) sec_##n,
    DEBUGINFO_SECTIONS
    count_debuginfo_sections,
#undef SEC

    /* Non-debuginfo sections:  */
    sec_rel = count_debuginfo_sections,
    sec_rela,

    // XXX the following should really be split out to different enum
    /* Non-sections:  */
    sec_locexpr,	/* Not a section, but a portion of file that
			   contains a location expression.  */
    rel_value,		/* For relocations, this denotes that the
			   relocation is applied to taget value, not a
			   section offset.  */
    rel_address,	/* Same as above, but for addresses.  */
    rel_exec,		/* Some as above, but we expect EXEC bit.  */
  };

  // section_name[0] is for sec_invalid.  The last index is for
  // count_debuginfo_sections and is NULL.
  extern char const *section_name[];

#ifdef __cplusplus
}
#endif

#endif//DWARFLINT_SECTION_ID_H
