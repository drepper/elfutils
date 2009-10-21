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

#ifndef DWARFLINT_HL_H
#define DWARFLINT_HL_H

#include "../libdw/libdw.h"
#include "../libebl/libebl.h"
#include "coverage.h"
#include "messages.h"
#include "readctx.h"
#include "addr-record.h"

#ifdef __cplusplus
extern "C"
{
#else
# include <stdbool.h>
#endif

  struct hl_ctx;
  struct relocation
  {
    uint64_t offset;
    uint64_t addend;
    int symndx;
    int type;
    bool invalid;	/* Whether this one relocation should be
			   ignored.  Necessary so that we don't report
			   invalid & missing relocation twice.  */
  };

  struct relocation_data
  {
    Elf_Data *symdata;		/* Symbol table associated with this
				   relocation section.  */
    size_t type;		/* SHT_REL or SHT_RELA.  */

    struct relocation *rel;	/* Array of relocations.  May be NULL
				   if there are no associated
				   relocation data.  */
    size_t size;
    size_t alloc;
    size_t index;		/* Current index. */
  };

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

  // xxx some of that will go away
  extern struct abbrev_table * abbrev_table_load (struct read_ctx *ctx);
  extern void abbrev_table_free (struct abbrev_table *abbr);
  extern struct abbrev *abbrev_table_find_abbrev (struct abbrev_table *abbrevs,
						  uint64_t abbrev_code);
  extern bool read_rel (struct elf_file *file, struct sec *sec,
			Elf_Data *reldata, bool elf_64);
  extern bool address_aligned (uint64_t addr, uint64_t align);
  extern bool necessary_alignment (uint64_t start, uint64_t length,
				   uint64_t align);

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
  extern struct cu * check_info_structural (struct elf_file *file,
					    struct sec *sec,
					    struct abbrev_table *abbrev_chain,
					    Elf_Data *strings,
					    struct cu_coverage *cu_coverage);
  extern bool check_loc_or_range_structural (struct elf_file *file,
					     struct sec *sec,
					     struct cu *cu_chain,
					     struct cu_coverage *cu_coverage);
  extern bool check_aranges_structural (struct elf_file *file,
					struct sec *sec,
					struct cu *cu_chain,
					struct coverage *coverage);
  extern bool check_pub_structural (struct elf_file *file,
				    struct sec *sec,
				    struct cu *cu_chain);
  extern bool check_line_structural (struct elf_file *file,
				     struct sec *sec,
				     struct addr_record *line_tables);
  extern void cu_free (struct cu *cu_chain);


  struct hole_info
  {
    enum section_id section;
    enum message_category category;
    void *data;
    unsigned align;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->data has to point at d_buf of section in question.  */
  bool found_hole (uint64_t begin, uint64_t end, void *data);

  struct coverage_map_hole_info
  {
    struct elf_file *elf;
    struct hole_info info;
  };

  /* DATA has to be a pointer to an instance of struct hole_info.
     DATA->info.data has to be NULL, it is used by the callback.  */
  bool coverage_map_found_hole (uint64_t begin, uint64_t end,
				struct section_coverage *sco, void *data);


  struct abbrev
  {
    uint64_t code;
    struct where where;

    /* Attributes.  */
    struct abbrev_attrib
    {
      struct where where;
      uint16_t name;
      uint8_t form;
    } *attribs;
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

  struct abbrev_table
  {
    struct abbrev_table *next;
    struct abbrev *abbr;
    uint64_t offset;
    size_t size;
    size_t alloc;
    bool used;		/* There are CUs using this table.  */
    bool skip_check;	/* There were errors during loading one of the
			   CUs that use this table.  Check for unused
			   abbrevs should be skipped.  */
  };

  struct cu
  {
    struct cu *next;
    uint64_t offset;
    uint64_t cudie_offset;
    uint64_t length;
    uint64_t low_pc;              // DW_AT_low_pc value of CU DIE, -1 if not present.
    struct addr_record die_addrs; // Addresses where DIEs begin in this CU.
    struct ref_record die_refs;   // DIE references into other CUs from this CU.
    struct ref_record loc_refs;   // references into .debug_loc from this CU.
    struct ref_record range_refs; // references into .debug_ranges from this CU.
    struct ref_record line_refs;	// references into .debug_line from this CU.
    struct where where;           // Where was this section defined.
    int address_size;             // Address size in bytes on the target machine.
    int offset_size;		// Offset size in this CU.
    int version;			// CU version
    bool has_arange;              // Whether we saw arange section pointing to this CU.
    bool has_pubnames;            // Likewise for pubnames.
    bool has_pubtypes;            // Likewise for pubtypes.
  };

#ifdef __cplusplus
}
#endif

#endif/*DWARFLINT_HL_H*/
