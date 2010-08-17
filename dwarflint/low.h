/* Pedantic checking of DWARF files.
   Copyright (C) 2008,2009 Red Hat, Inc.
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

#ifndef DWARFLINT_LOW_H
#define DWARFLINT_LOW_H

#include "../libdw/libdw.h"
#include "../libebl/libebl.h"
#include "coverage.h"
#include "messages.h"
#include "readctx.h"
#include "addr-record.h"
#include "reloc.h"
#include "tables.h"

#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  struct hl_ctx;

  struct sec
  {
    GElf_Shdr shdr;
    struct relocation_data rel;
    Elf_Scn *scn;
    const char *name;

    Elf_Data *data;	/* May be NULL if data in this section are
			   missing or not substantial.  */
    enum section_id id;
  };

  struct elf_file
  {
    GElf_Ehdr ehdr;	/* Header of underlying Elf.  */
    Elf *elf;
    Ebl *ebl;

    struct sec *sec;	/* Array of sections.  */
    size_t size;
    size_t alloc;

    /* Pointers into SEC above.  Maps section_id to section.  */
    struct sec *debugsec[count_debuginfo_sections];

    bool addr_64;	/* True if it's 64-bit Elf.  */
    bool other_byte_order; /* True if the file has a byte order
			      different from the host.  */
  };

  /* Check that .debug_aranges and .debug_ranges match.  */
  extern struct hl_ctx *hl_ctx_new (Elf *elf);
  extern void hl_ctx_delete (struct hl_ctx *hlctx);
  extern bool check_expected_trees (struct hl_ctx *hlctx);
  extern bool elf_file_init (struct elf_file *file, Elf *elf);

  struct abbrev_table
  {
    struct abbrev_table *next;
    struct abbrev *abbr;
    uint64_t offset;
    size_t size;
    size_t alloc;
    bool used;		/* There are CUs using this table.  */
  };

  // xxx some of that will go away
  extern struct abbrev *abbrev_table_find_abbrev (struct abbrev_table const *abbrevs,
						  uint64_t abbrev_code);
  extern bool read_rel (struct elf_file *file, struct sec *sec,
			Elf_Data *reldata, bool elf_64);
  extern bool address_aligned (uint64_t addr, uint64_t align);
  extern bool necessary_alignment (uint64_t start, uint64_t length,
				   uint64_t align);
  extern bool read_size_extra (struct read_ctx *ctx, uint32_t size32,
			       uint64_t *sizep, int *offset_sizep,
			       struct where *wh);
#define PRI_NOT_ENOUGH ": not enough data for %s.\n"
  extern bool supported_version (unsigned version,
				 size_t num_supported, struct where *where, ...);
  extern bool check_zero_padding (struct read_ctx *ctx,
				  enum message_category category,
				  struct where const *wh);
  extern bool read_address_size (bool elf_64,
				 struct read_ctx *ctx,
				 int *address_sizep,
				 struct where const *where);

  struct section_coverage
  {
    struct sec *sec;
    struct coverage cov;
    bool hit; /* true if COV is not pristine.  */
    bool warn; /* dwarflint should emit a warning if a coverage
		  appears in this section */
  };

  struct coverage_map
  {
    struct elf_file *elf;
    struct section_coverage *scos;
    size_t size;
    size_t alloc;
    bool allow_overlap;
  };

  struct cu_coverage
  {
    struct coverage cov;
    bool need_ranges;	/* If all CU DIEs have high_pc/low_pc
			   attribute pair, we don't need separate
			   range pass.  Otherwise we do.  As soon as
			   ranges are projected into cov, the flag
			   is set to false again.  */
  };

  // xxx low-level check entry points, will go away
  struct cu;
  extern bool check_aranges_structural (struct elf_file *file,
					struct sec *sec,
					struct cu *cu_chain,
					struct coverage *coverage);
  extern bool check_line_structural (struct elf_file *file,
				     struct sec *sec,
				     struct addr_record *line_tables);
  extern void check_range_relocations (enum message_category cat,
				       struct where *where,
				       struct elf_file const *file,
				       GElf_Sym *begin_symbol,
				       GElf_Sym *end_symbol,
				       const char *description);

  extern int check_sibling_form (dwarf_version_h ver, uint64_t form);
  extern bool is_location_attrib (uint64_t name);

  bool checked_read_uleb128 (struct read_ctx *ctx, uint64_t *ret,
			     struct where *where, const char *what);
  bool checked_read_sleb128 (struct read_ctx *ctx, int64_t *ret,
			     struct where *where, const char *what);

  struct abbrev_attrib
  {
    struct where where;
    uint16_t name;
    uint8_t form;
  };

  struct abbrev
  {
    uint64_t code;
    struct where where;

    /* Attributes.  */
    struct abbrev_attrib *attribs;
    size_t size;
    size_t alloc;

    /* While ULEB128 can hold numbers > 32bit, these are not legal
       values of many enum types.  So just use as large type as
       necessary to cover valid values.  */
    uint16_t tag;
    bool has_children;

    /* Whether some DIE uses this abbrev.  */
    bool used;
  };

  struct cu_head
  {
    uint64_t offset;
    Dwarf_Off size;               // Size of this CU.
    Dwarf_Off head_size;          // Size from begin to 1st byte of CU.
    Dwarf_Off total_size;         // size + head_size

    int offset_size;		  // Offset size in this CU.
    struct where where;           // Where was this section defined.
    Dwarf_Off abbrev_offset;      // Abbreviation section that this CU uses.
    int version;                  // CU version
    int address_size;             // Address size in bytes on the target machine.
  };

  struct cu
  {
    struct cu *next;              // For compatibility with C level.
                                  // xxx will probably go away eventually
    struct cu_head const *head;
    uint64_t cudie_offset;
    uint64_t low_pc;              // DW_AT_low_pc value of CU DIE, -1 if not present.
    struct ref stmt_list;
    struct addr_record die_addrs; // Addresses where DIEs begin in this CU.
    struct ref_record die_refs;   // DIE references into other CUs from this CU.
    struct ref_record loc_refs;   // references into .debug_loc from this CU.
    struct ref_record range_refs; // references into .debug_ranges from this CU.
    struct ref_record decl_file_refs;  // values of DW_AT_decl_file in this CU.
    bool has_arange;              // Whether we saw arange section pointing at this CU.
    bool has_pubnames;            // Likewise for pubnames.
    bool has_pubtypes;            // Likewise for pubtypes.
  };

#ifdef __cplusplus
}
#endif

#endif/*DWARFLINT_LOW_H*/
