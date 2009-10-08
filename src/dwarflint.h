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
#include "dwarflint-coverage.h"
#include "dwarflint-messages.h"

#ifdef __cplusplus
# include <string>
extern "C"
{
#else
# include <stdbool.h>
#endif

  /* Entry points for high-level checks.  */

  struct hl_ctx;

  /* Check that .debug_aranges and .debug_ranges match.  */
  extern struct hl_ctx *hl_ctx_new (Elf *elf);
  extern void hl_ctx_delete (struct hl_ctx *hlctx);
  extern bool check_matching_ranges (struct hl_ctx *hlctx);
  extern bool check_expected_trees (struct hl_ctx *hlctx);
  extern bool check_range_out_of_scope (struct hl_ctx *hlctx);
  extern void process_file (Elf *elf, const char *fname, bool only_one);

  /* Whole-program options.  */
  extern bool tolerate_nodebug;
  extern bool be_quiet; /* -q */
  extern bool be_verbose; /* -v */
  extern bool be_strict; /* --strict */
  extern bool be_gnu; /* --gnu */
  extern bool be_tolerant; /* --tolerant */
  extern bool show_refs; /* --ref */
  extern bool do_high_level; /* ! --nohl */
  extern bool dump_die_offsets; /* --dump-offsets */

  struct relocation
  {
    uint64_t offset;
    uint64_t addend;
    int symndx;
    int type;
    bool invalid;		/* Whether this one relocation should be
				   ignored.  Necessary so that we don't
				   double-report invalid & missing
				   relocation.  */
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

  void section_coverage_init (struct section_coverage *sco,
			      struct sec *sec, bool warn);
  bool coverage_map_init (struct coverage_map *coverage_map,
			  struct elf_file *elf,
			  Elf64_Xword mask,
			  Elf64_Xword warn_mask,
			  bool allow_overlap);
  void coverage_map_add (struct coverage_map *coverage_map,
			 uint64_t address, uint64_t length,
			 struct where *where, enum message_category cat);
  bool coverage_map_find_holes (struct coverage_map *coverage_map,
				bool (*cb) (uint64_t, uint64_t,
					    struct section_coverage *, void *),
				void *user);
  void coverage_map_free (struct coverage_map *coverage_map);


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


#ifdef __cplusplus
}
#endif

#endif/*DWARFLINT_HL_H*/
