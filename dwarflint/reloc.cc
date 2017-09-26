/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010,2011 Red Hat, Inc.
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

// xxx drop as soon as not necessary
#define __STDC_FORMAT_MACROS

#include "reloc.hh"
#include "elf_file.hh"
#include "messages.hh"
#include "misc.hh"
#include "readctx.hh"
#include "pri.hh"

#include <sstream>
#include <libebl.h>
#include <cassert>
#include <cinttypes>

namespace
{
  class reloc_locus
    : public locus
  {
    locus const &_m_ref;
    size_t _m_index;
    uint64_t _m_offset;
    int _m_type;

  public:
    reloc_locus (int type, locus const &ref, uint64_t offset)
      : _m_ref (ref)
      , _m_index (-1)
      , _m_offset (offset)
      , _m_type (type)
    {
    }

    reloc_locus (int type, locus const &ref, unsigned index)
      : _m_ref (ref)
      , _m_index (index)
      , _m_offset (-1)
      , _m_type (type)
    {
    }

    void
    set_offset (uint64_t offset)
    {
      _m_offset = offset;
    }

    virtual std::string
    format (bool) const
    {
      std::stringstream ss;
      ss << (_m_type == SHT_REL ? ".rel" : ".rela") << " ";
      if (_m_offset != (uint64_t)-1)
	ss << pri::hex (_m_offset);
      else
	{
	  assert (_m_index != (size_t)-1);
	  ss << "#" << _m_index;
	}

      // Do non-brief formatting of referee
      ss << " of " << _m_ref.format ();
      return ss.str ();
    }
  };
}

relocation *
relocation_next (relocation_data *reloc, uint64_t offset,
		 locus const &loc, enum skip_type st)
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
	      reloc_locus reloc_where (reloc->type, loc, rel->offset);
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
		 locus const &loc, enum skip_type st)
{
  if (reloc != NULL && reloc->rel != NULL)
    relocation_next (reloc, offset - 1, loc, st);
}

void
relocation_reset (struct relocation_data *reloc)
{
  if (reloc != NULL)
    reloc->index = 0;
}

/* Skip all the remaining relocations.  */
void
relocation_skip_rest (relocation_data *reloc,
		      locus const &loc)
{
  if (reloc->rel != NULL)
    relocation_next (reloc, (uint64_t)-1, loc, skip_mismatched);
}

static void
do_one_relocation (elf_file const *file,
		   relocation_data *reloc,
		   relocation *rel,
		   unsigned rel_width,
		   uint64_t *value,
		   reloc_locus const &reloc_where,
		   rel_target reltgt,
		   GElf_Sym *symbol,
		   GElf_Sym **symptr)
{
#define require(T, STREAMOPS)			\
  do {						\
    if (!(T))					\
      {						\
	wr_error (reloc_where) << STREAMOPS	\
			       << std::endl;	\
	return;					\
      }						\
  } while (0)

#define require_valid_section_index					\
  require (section_index_valid,						\
	   "invalid associated section #" << section_index << '.')

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

  GElf_Section section_index = symbol->st_shndx;
  /* XXX We should handle SHN_XINDEX here.  Or, instead, maybe it
     would be possible to use dwfl, which already does XINDEX
     translation.  */

  if (section_index == 0)
    {
	wr_error (reloc_where)
	  << "relocation refers to an undefined symbol."
	  << std::endl;
	return;
    }

  // Valid in the sense that it can be used as an index to file->sec
  bool section_index_valid = section_index < file->size;

  /* For ET_REL files, we do section layout manually.  But we
     don't update symbol table doing that.  So instead of looking
     at symbol value, look at section address.  */
  GElf_Addr sym_value = symbol->st_value;
  if (file->ehdr.e_type == ET_REL
      && ELF64_ST_TYPE (symbol->st_info) == STT_SECTION)
    {
      if (sym_value != 0)
	wr_message (reloc_where, mc_reloc | mc_impact_1)
	  << "relocation formed using STT_SECTION symbol with non-zero value."
	  << std::endl;

      require_valid_section_index;
      sym_value = file->sec[section_index].shdr.sh_addr;
    }

  /* It's target value, not section offset.  */
  if (reltgt == rel_target::rel_value
      || reltgt == rel_target::rel_address
      || reltgt == rel_target::rel_exec)
    {
      /* If a target value is what's expected, then complain if
	 it's not either SHN_ABS, an SHF_ALLOC section, or
	 SHN_UNDEF.  For data forms of address_size, an SHN_UNDEF
	 reloc is acceptable, otherwise reject it.  */
      if (!(section_index == SHN_ABS
	    || (reltgt == rel_target::rel_address
		&& (section_index == SHN_UNDEF
		    || section_index == SHN_COMMON))))
	{
	  if (reltgt != rel_target::rel_address && section_index == SHN_UNDEF)
	    wr_error (&reloc_where,
		      ": relocation of an address is formed using SHN_UNDEF symbol"
		      " (symtab index %d).\n", rel->symndx);
	  else
	    {
	      require_valid_section_index;
	      GElf_Shdr *shdr = &file->sec[section_index].shdr;
	      if ((shdr->sh_flags & SHF_ALLOC) != SHF_ALLOC)
		wr_message (mc_reloc | mc_impact_3, &reloc_where,
			    ": associated section %s isn't SHF_ALLOC.\n",
			    file->sec[section_index].name);
	      if (reltgt == rel_target::rel_exec
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
      require_valid_section_index;
      section_id secid = file->sec[section_index].id;
      if (reltgt != secid)
	// If symtab[symndx].st_shndx does not match the expected
	// debug section's index, complain.
	wr_error (reloc_where)
	  << "relocation references section "
	  << (file->sec[section_index].name ?: "<invalid>") << ", but "
	  << section_locus (secid) << " was expected." << std::endl;
    }

  /* Only do the actual relocation if we have ET_REL files.  For
     non-ET_REL files, only do the above checking.  */
  if (file->ehdr.e_type == ET_REL)
    {
      *value = rel->addend + sym_value;
      if (rel_width == 4)
	*value = *value & (uint64_t)(uint32_t)-1;
    }

#undef require_valid_section_index
#undef require
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
	      locus const &loc,
	      rel_target reltgt,
	      GElf_Sym **symptr)
{
  if (rel->invalid)
    return;

  reloc_locus reloc_where (reloc->type, loc, rel->offset);

  GElf_Sym symbol_mem, *symbol;
  if (symptr != NULL)
    {
      symbol = *symptr;
      *symptr = NULL;
    }
  else
    symbol = &symbol_mem;

  if (reltgt == sec_invalid)
    {
      wr_message (reloc_where, mc_impact_3 | mc_reloc)
	<< "relocates a datum that shouldn't be relocated.\n";
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
    wr_error (reloc_where)
      << rel_width << "-byte relocation relocates "
      << width << "-byte datum.\n";

  // Tolerate if we failed to obtain the symbol table.
  if (reloc->symdata != NULL)
    do_one_relocation (file, reloc, rel, rel_width, value,
		       reloc_where, reltgt, symbol, symptr);
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

  section_locus parent (sec->id);

  for (unsigned i = 0; i < count; ++i)
    {
      reloc_locus where (sec->rel.type, parent, i);

      REALLOC (&sec->rel, rel);
      struct relocation *cur = sec->rel.rel + sec->rel.size++;
      new (cur) relocation ();

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

      where.set_offset (cur->offset);

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
			 PRIx64 ", value=%#" PRIx64 ").\n", rela->r_addend, value);
	  cur->addend = rela->r_addend;
	}
      else
	cur->addend = value;
    }

  qsort (sec->rel.rel, sec->rel.size,
	 sizeof (*sec->rel.rel), &compare_rel);
  return true;
}
