#include "reloc.h"
#include "messages.h"
#include "low.h"
#include <sstream>
#include <libebl.h>
#include <cassert>

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
		    : "relocation is mismatched.")
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
relocate_one (struct elf_file *file,
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
