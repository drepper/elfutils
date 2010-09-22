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

// xxx drop as soon as not necessary
#define __STDC_FORMAT_MACROS

#include "low.h"
#include "reloc.hh"
#include "messages.hh"
#include "misc.h"
#include "readctx.h"

#include <sstream>
#include <libebl.h>
#include <cassert>
#include <inttypes.h>

static struct where
where_from_reloc (struct relocation_data *reloc, struct where const *ref)
{
  struct where where
    = WHERE (reloc->type == SHT_REL ? sec_rel : sec_rela, NULL);
  where_reset_1 (&where, reloc->rel[reloc->index].offset);
  where.ref = ref;
  return where;
}

relocation *
relocation_next (relocation_data *reloc, uint64_t offset,
		 struct where const *where, enum skip_type st)
{
  if (reloc == NULL || reloc->rel == NULL)
    return NULL;

  while (reloc->index < reloc->size)
    {
      struct relocation *rel = reloc->rel + reloc->index;

      /* This relocation entry is ahead of us.  */
      if (rel->offset > offset)
	return NULL;

      reloc->index++;

      if (rel->invalid)
	continue;

      if (rel->offset < offset)
	{
	  if (st != skip_ok)
	    {
	      struct where reloc_where = where_from_reloc (reloc, where);
	      where_reset_2 (&reloc_where, rel->offset);
	      wr_error (reloc_where)
		<< (st == skip_unref
		    ? "relocation targets unreferenced portion of the section."
		    : (assert (st == skip_mismatched),
		       "relocation relocates unknown datum."))
		<< std::endl;
	    }
	  continue;
	}

      return rel;
    }

  return NULL;
}

/* Skip all relocation up to offset, and leave cursor pointing at that
   relocation, so that next time relocation_next is called, relocation
   matching that offset is immediately yielded.  */
void
relocation_skip (struct relocation_data *reloc, uint64_t offset,
		 struct where const *where, enum skip_type st)
{
  if (reloc != NULL && reloc->rel != NULL)
    relocation_next (reloc, offset - 1, where, st);
}

void
relocation_reset (struct relocation_data *reloc)
{
  if (reloc != NULL)
    reloc->index = 0;
}

/* Skip all the remaining relocations.  */
void
relocation_skip_rest (struct relocation_data *reloc,
		      enum section_id id)
{
  if (reloc->rel != NULL)
    {
      where wh = WHERE (id, NULL);
      relocation_next (reloc, (uint64_t)-1, &wh,
		       skip_mismatched);
    }
}

/* SYMPTR may be NULL, otherwise (**SYMPTR) has to yield valid memory
   location.  When the function returns, (*SYMPTR) is either NULL, in
   which case we failed or didn't get around to obtain the symbol from
   symbol table, or non-NULL, in which case the symbol was initialized.  */
void
relocate_one (struct elf_file const *file,
	      struct relocation_data *reloc,
	      struct relocation *rel,
	      unsigned width, uint64_t *value,
	      struct where const *where,
	      enum section_id offset_into, GElf_Sym **symptr)
{
  if (rel->invalid)
    return;

  struct where reloc_where = where_from_reloc (reloc, where);
  where_reset_2 (&reloc_where, rel->offset);
  struct where reloc_ref_where = reloc_where;
  reloc_ref_where.next = where;

  GElf_Sym symbol_mem, *symbol;
  if (symptr != NULL)
    {
      symbol = *symptr;
      *symptr = NULL;
    }
  else
    symbol = &symbol_mem;

  if (offset_into == sec_invalid)
    {
      wr_message (mc_impact_3 | mc_reloc, &reloc_ref_where,
		  ": relocates a datum that shouldn't be relocated.\n");
      return;
    }

  Elf_Type type = ebl_reloc_simple_type (file->ebl, rel->type);

  unsigned rel_width;
  switch (type)
    {
    case ELF_T_BYTE:
      rel_width = 1;
      break;

    case ELF_T_HALF:
      rel_width = 2;
      break;

    case ELF_T_WORD:
    case ELF_T_SWORD:
      rel_width = 4;
      break;

    case ELF_T_XWORD:
    case ELF_T_SXWORD:
      rel_width = 8;
      break;

    default:
      /* This has already been diagnosed during the isolated
	 validation of relocation section.  */
      return;
    };

  if (rel_width != width)
    wr_error (&reloc_ref_where,
	      ": %d-byte relocation relocates %d-byte datum.\n",
	      rel_width, width);

  /* Tolerate that we might have failed to obtain the symbol table.  */
  if (reloc->symdata != NULL)
    {
      symbol = gelf_getsym (reloc->symdata, rel->symndx, symbol);
      if (symptr != NULL)
	*symptr = symbol;
      if (symbol == NULL)
	{
	  wr_error (&reloc_where,
		    ": couldn't obtain symbol #%d: %s.\n",
		    rel->symndx, elf_errmsg (-1));
	  return;
	}

      uint64_t section_index = symbol->st_shndx;
      /* XXX We should handle SHN_XINDEX here.  Or, instead, maybe it
	 would be possible to use dwfl, which already does XINDEX
	 translation.  */

      /* For ET_REL files, we do section layout manually.  But we
	 don't update symbol table doing that.  So instead of looking
	 at symbol value, look at section address.  */
      uint64_t sym_value = symbol->st_value;
      if (file->ehdr.e_type == ET_REL
	  && ELF64_ST_TYPE (symbol->st_info) == STT_SECTION)
	{
	  assert (sym_value == 0);
	  sym_value = file->sec[section_index].shdr.sh_addr;
	}

      /* It's target value, not section offset.  */
      if (offset_into == rel_value
	  || offset_into == rel_address
	  || offset_into == rel_exec)
	{
	  /* If a target value is what's expected, then complain if
	     it's not either SHN_ABS, an SHF_ALLOC section, or
	     SHN_UNDEF.  For data forms of address_size, an SHN_UNDEF
	     reloc is acceptable, otherwise reject it.  */
	  if (!(section_index == SHN_ABS
		|| (offset_into == rel_address
		    && (section_index == SHN_UNDEF
			|| section_index == SHN_COMMON))))
	    {
	      if (offset_into != rel_address && section_index == SHN_UNDEF)
		wr_error (&reloc_where,
			    ": relocation of an address is formed against SHN_UNDEF symbol"
			    " (symtab index %d).\n", rel->symndx);
	      else
		{
		  GElf_Shdr *shdr = &file->sec[section_index].shdr;
		  if ((shdr->sh_flags & SHF_ALLOC) != SHF_ALLOC)
		    wr_message (mc_reloc | mc_impact_3, &reloc_where,
				": associated section %s isn't SHF_ALLOC.\n",
				file->sec[section_index].name);
		  if (offset_into == rel_exec
		      && (shdr->sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
		    /* This may still be kosher, but it's suspicious.  */
		    wr_message (mc_reloc | mc_impact_2, &reloc_where,
				": relocation against %s is suspicious, expected executable section.\n",
				file->sec[section_index].name);
		}
	    }
	}
      else
	{
	  enum section_id id;
	  /* If symtab[symndx].st_shndx does not match the expected
	     debug section's index, complain.  */
	  if (section_index >= file->size)
	    wr_error (reloc_where)
	      << "invalid associated section #" << section_index
	      << '.' << std::endl;
	  else if ((id = file->sec[section_index].id) != offset_into)
	    wr_error (reloc_where)
	      << "relocation references section "
	      << file->sec[section_index].name << ", but "
	      << WHERE (offset_into, NULL) << " was expected." << std::endl;
	}

      /* Only do the actual relocation if we have ET_REL files.  For
	 non-ET_REL files, only do the above checking.  */
      if (file->ehdr.e_type == ET_REL)
	{
	  *value = rel->addend + sym_value;
	  if (rel_width == 4)
	    *value = *value & (uint64_t)(uint32_t)-1;
	}
    }
}

static GElf_Rela *
get_rel_or_rela (Elf_Data *data, int ndx,
		 GElf_Rela *dst, size_t type)
{
  if (type == SHT_RELA)
    return gelf_getrela (data, ndx, dst);
  else
    {
      assert (type == SHT_REL);
      GElf_Rel rel_mem;
      if (gelf_getrel (data, ndx, &rel_mem) == NULL)
	return NULL;
      dst->r_offset = rel_mem.r_offset;
      dst->r_info = rel_mem.r_info;
      dst->r_addend = 0;
      return dst;
    }
}

/* Sort the reloc section so that the applicable addresses of
   relocation entries are monotonously increasing.  */
static int
compare_rel (const void *a, const void *b)
{
  return ((struct relocation *)a)->offset
    - ((struct relocation *)b)->offset;
}

bool
read_rel (struct elf_file *file,
	  struct sec *sec,
	  Elf_Data *reldata,
	  bool elf_64)
{
  assert (sec->rel.type == SHT_REL
	  || sec->rel.type == SHT_RELA);
  bool is_rela = sec->rel.type == SHT_RELA;

  struct read_ctx ctx;
  read_ctx_init (&ctx, sec->data, file->other_byte_order);

  size_t entrysize
    = elf_64
    ? (is_rela ? sizeof (Elf64_Rela) : sizeof (Elf64_Rel))
    : (is_rela ? sizeof (Elf32_Rela) : sizeof (Elf32_Rel));
  size_t count = reldata->d_size / entrysize;

  struct where parent = WHERE (sec->id, NULL);
  struct where where = WHERE (is_rela ? sec_rela : sec_rel, NULL);
  where.ref = &parent;

  for (unsigned i = 0; i < count; ++i)
    {
      where_reset_1 (&where, i);

      REALLOC (&sec->rel, rel);
      struct relocation *cur = sec->rel.rel + sec->rel.size++;
      WIPE (*cur);

      GElf_Rela rela_mem, *rela
	= get_rel_or_rela (reldata, i, &rela_mem, sec->rel.type);
      if (rela == NULL)
	{
	  wr_error (&where, ": couldn't read relocation.\n");
	skip:
	  cur->invalid = true;
	  continue;
	}

      int cur_type = GELF_R_TYPE (rela->r_info);
      if (cur_type == 0) /* No relocation.  */
	{
	  wr_message (mc_impact_3 | mc_reloc | mc_acc_bloat, &where,
		      ": NONE relocation is superfluous.\n");
	  goto skip;
	}

      cur->offset = rela->r_offset;
      cur->symndx = GELF_R_SYM (rela->r_info);
      cur->type = cur_type;

      where_reset_2 (&where, cur->offset);

      Elf_Type type = ebl_reloc_simple_type (file->ebl, cur->type);
      int width;

      switch (type)
	{
	case ELF_T_WORD:
	case ELF_T_SWORD:
	  width = 4;
	  break;

	case ELF_T_XWORD:
	case ELF_T_SXWORD:
	  width = 8;
	  break;

	case ELF_T_BYTE:
	case ELF_T_HALF:
	  /* Technically legal, but never used.  Better have dwarflint
	     flag them as erroneous, because it's more likely these
	     are a result of a bug than actually being used.  */
	  {
	    char buf[64];
	    wr_error (&where, ": 8 or 16-bit relocation type %s.\n",
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }

	default:
	  {
	    char buf[64];
	    wr_error (&where, ": invalid relocation %d (%s).\n",
		      cur->type,
		      ebl_reloc_type_name (file->ebl, cur->type,
					   buf, sizeof (buf)));
	    goto skip;
	  }
	};

      if (cur->offset + width >= sec->data->d_size)
	{
	  wr_error (&where,
		    ": relocation doesn't fall into relocated section.\n");
	  goto skip;
	}

      uint64_t value;
      if (width == 4)
	value = dwarflint_read_4ubyte_unaligned
	  ((char *)sec->data->d_buf + cur->offset, file->other_byte_order);
      else
	{
	  assert (width == 8);
	  value = dwarflint_read_8ubyte_unaligned
	    ((char *)sec->data->d_buf + cur->offset, file->other_byte_order);
	}

      if (is_rela)
	{
	  if (value != 0)
	    wr_message (mc_impact_2 | mc_reloc, &where,
			": SHR_RELA relocates a place with non-zero value (addend=%#"
			PRIx64", value=%#"PRIx64").\n", rela->r_addend, value);
	  cur->addend = rela->r_addend;
	}
      else
	cur->addend = value;
    }

  qsort (sec->rel.rel, sec->rel.size,
	 sizeof (*sec->rel.rel), &compare_rel);
  return true;
}
