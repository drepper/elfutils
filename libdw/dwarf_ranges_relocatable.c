/* Enumerate the PC ranges covered by a DIE.
   Copyright (C) 2010 Red Hat, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "relocate.h"
#include <dwarf.h>
#include <assert.h>

ptrdiff_t
internal_function
__libdw_ranges_relocatable (struct Dwarf_CU *cu, Dwarf_Attribute *attr,
			    ptrdiff_t offset,
			    Dwarf_Relocatable *basep,
			    Dwarf_Relocatable *startp, Dwarf_Relocatable *endp,
			    Dwarf_Block *exprloc)
{
  if (offset == 1)
    return 0;

  const unsigned int sec_idx = (exprloc != NULL ? IDX_debug_loc
				: IDX_debug_ranges);
  unsigned char *readp;
  unsigned char *readendp;
  if (offset == 0)
    {
      Dwarf_Word start_offset;
      if ((readp = __libdw_formptr (attr, sec_idx,
				    exprloc != NULL ? DWARF_E_NO_LOCLIST
				    : DWARF_E_NO_DEBUG_RANGES,
				    &readendp, &start_offset)) == NULL)
	return -1;

      offset = start_offset;
      assert ((Dwarf_Word) offset == start_offset);

      /* This is a marker that we need to fetch the CU base address.  */
      basep->cu = NULL;
    }
  else if (__libdw_offset_in_section (cu->dbg,
				      exprloc != NULL ? IDX_debug_loc
				      : IDX_debug_ranges,
				      offset, 1))
    return -1l;
  else
    {
      const Elf_Data *d = cu->dbg->sectiondata[sec_idx];
      readp = d->d_buf + offset;
      readendp = d->d_buf + d->d_size;
    }

  inline void store (Dwarf_Relocatable *relocp, ptrdiff_t read_adjust)
  {
    *relocp = (Dwarf_Relocatable)
      {
	.cu = cu, .sec = sec_idx, .form = DW_FORM_addr,
	.valp = readp
      };
    readp += read_adjust;
  }

  inline bool finalize_reloc (Dwarf_Relocatable *reloc, unsigned int width)
  {
    if (basep->cu == NULL)
      {
	/* This is the initial default base address and we have not used it
	   yet.  Find the base address of the compilation unit.  It will
	   normally be specified by DW_AT_low_pc.  In DWARF-3 draft 4, the
	   base address could be overridden by DW_AT_entry_pc.  It's been
	   removed, but GCC emits DW_AT_entry_pc and not DW_AT_lowpc for
	   compilation units with discontinuous ranges.  */

	Dwarf_Die cudie = CUDIE (cu);
	Dwarf_Attribute loattr;
	if (unlikely (INTUSE(dwarf_form_relocatable)
		      (INTUSE(dwarf_attr) (&cudie, DW_AT_low_pc, &loattr)
		       ?: INTUSE(dwarf_attr) (&cudie, DW_AT_entry_pc, &loattr),
		       basep) < 0))
	  return true;
      }

    GElf_Sxword addend;
    if (__libdw_relocatable (cu->dbg, sec_idx, reloc->valp, width,
			     NULL, &addend) > 0)
      {
	/* The address itself has a relocation.
	   The base address entry must not also have a relocation.  */

	if (unlikely (__libdw_relocatable (cu->dbg, sec_idx, basep->valp, width,
					   NULL, &addend) > 0))
	  {
	    __libdw_seterrno (DWARF_E_INVALID_DWARF);
	    return true;
	  }
      }
    else
      /* The address is relative to the base address relocation.  */
      reloc->valp = basep->valp;

    reloc->adjust = addend;

    return false;
  }

  inline ptrdiff_t finalize (unsigned int width)
  {
    if (finalize_reloc (startp, width) || finalize_reloc (endp, width))
      return -1;
    return readp - (unsigned char *) cu->dbg->sectiondata[sec_idx]->d_buf;
  }

#define READ_RANGES(AS, Addr, READ)					      \
  while (likely (readendp - readp >= 2 * AS))				      \
    {									      \
      Addr begin = READ (readp);					      \
									      \
      if (begin == (Addr) -1)						      \
	{								      \
	  /* If this is unrelocated, this is a base address entry.  */	      \
	  int result = __libdw_relocatable (cu->dbg, sec_idx, readp, AS,      \
					    NULL, NULL);		      \
	  if (unlikely (result < 0)) /* Indigestion.  */		      \
	    return -1;							      \
	  if (result == 0)	/* Not relocatable. */			      \
	    {								      \
	      readp += AS;						      \
	      store (basep, AS);					      \
	      continue;							      \
	    }								      \
	}								      \
      else if (begin == 0 && READ (readp + AS) == 0)			      \
	{								      \
	  /* If these are both unrelocated, this is the end of list entry.  */\
	  int result = __libdw_relocatable (cu->dbg, sec_idx, readp, AS,      \
					    NULL, NULL);		      \
	  if (result == 0)		/* Not relocatable. */		      \
	    result = __libdw_relocatable (cu->dbg, sec_idx, readp + AS, AS,   \
					  NULL, NULL);			      \
	  if (unlikely (result < 0))	/* Indigestion.  */		      \
	    return -1;							      \
	  if (result == 0)	/* Not relocatable: end of list entry.  */    \
	    return 0;							      \
	}								      \
									      \
      /* This is a pair of addresses.  */				      \
      store (startp, AS);						      \
      store (endp, AS);							      \
									      \
      if (exprloc != NULL)						      \
	{								      \
	  if (unlikely (readendp - readp < 2))				      \
	    break;							      \
	  exprloc->length = read_2ubyte_unaligned_inc (cu->dbg, readp);	      \
	  if (unlikely ((size_t) (readendp - readp) < exprloc->length))	      \
	    break;							      \
	  exprloc->data = readp;					      \
	}								      \
									      \
      return finalize (AS);						      \
    }

  if (cu->address_size == 8)
    READ_RANGES (8, Elf64_Addr, read_8ubyte_unaligned_noncvt)
  else
    READ_RANGES (4, Elf32_Addr, read_4ubyte_unaligned_noncvt)

  __libdw_seterrno (DWARF_E_INVALID_DWARF);
  return -1;
}

ptrdiff_t
dwarf_ranges_relocatable (Dwarf_Die *die, ptrdiff_t offset,
			  Dwarf_Relocatable *basep,
			  Dwarf_Relocatable *startp, Dwarf_Relocatable *endp)
{
  if (die == NULL)
    return -1;

  if (offset != 0)
    /* Iterate from last position.  */
    return __libdw_ranges_relocatable (die->cu, NULL, offset,
				       basep, startp, endp, NULL);

  Dwarf_Attribute attr_mem;

  /* Usually there is a single contiguous range.  */
  if (INTUSE(dwarf_form_relocatable) (INTUSE(dwarf_attr) (die,
							  DW_AT_high_pc,
							  &attr_mem),
				      endp) == 0
      && INTUSE(dwarf_form_relocatable) (INTUSE(dwarf_attr) (die,
							     DW_AT_low_pc,
							     &attr_mem),
					 startp) == 0)
    /* A offset into .debug_ranges will never be 1, it must be at least a
       multiple of 4.  So we can return 1 as a special case value to mark
       there are no ranges to look for on the next call.  */
    return 1;

  Dwarf_Attribute *attr = INTUSE(dwarf_attr) (die, DW_AT_ranges, &attr_mem);
  if (attr == NULL)
    /* No PC attributes in this DIE at all, so an empty range list.  */
    return 0;

  return __libdw_ranges_relocatable (die->cu, attr, 0,
				     basep, startp, endp, NULL);
}
INTDEF (dwarf_ranges_relocatable)
