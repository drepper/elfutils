/* Relocation handling for DWARF reader.
   Copyright (C) 2010 Red Hat, Inc.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "relocate.h"
#include "../libebl/libebl.h"
#include <dwarf.h>
#include <assert.h>
#include <stdlib.h>


static int
internal_function
noresolve_symbol (bool undef,
		  Dwarf *dbg __attribute__ ((unused)),
		  GElf_Sym *sym __attribute__ ((unused)),
		  GElf_Word shndx __attribute__ ((unused)))
{
  __libdw_seterrno (undef ? DWARF_E_RELUNDEF : DWARF_E_RELOC);
  return -1;
}

void
internal_function
__libdw_relocate_begin (Dwarf *dbg, Elf_Scn *relscn[IDX_last], bool incomplete)
{
  if (incomplete)
    {
      /* We may need a second pass to identify some relocation sections.  */

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (dbg->elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr;
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  assert (shdr == &shdr_mem);
	  if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
	    for (size_t i = 0; i < IDX_last; ++i)
	      if (relscn[i] == NULL && dbg->sectiondata[i] != NULL
		  && (((Elf_Data_Scn * ) dbg->sectiondata[i])->s->index
		      == shdr->sh_link))
		relscn[i] = scn;
	}
    }

  dbg->relocate = libdw_typed_alloc (dbg, struct dwarf_file_reloc);
  dbg->relocate->ebl = NULL;
  dbg->relocate->resolve_symbol = &noresolve_symbol;

  /* All we do to start with is cache the section pointers.
     We'll do the rest on demand in digest_relocs, below.  */
  for (size_t i = 0; i < IDX_last; ++i)
    if (relscn[i] == NULL)
      dbg->relocate->sectionrel[i] = NULL;
    else
      (dbg->relocate->sectionrel[i]
       = libdw_typed_alloc (dbg, struct dwarf_section_reloc))->scn = relscn[i];
}

void
internal_function
__libdw_relocate_end (Dwarf *dbg)
{
  // XXX let dwfl preinstall, don't close here
  ebl_closebackend (dbg->relocate->ebl);
}

struct digested_reloc
{
  GElf_Sxword addend;
  const unsigned char *datum;
  int symndx;
  bool rel8;
};

static bool
match_r_type (const int *types, int r_type)
{
  for (const int *t = types; *t != 0; ++t)
    if (r_type == *t)
      return true;
  return false;
}

static inline int
digest_one_reloc (const int *rel8_types, const int *rel4_types,
		  struct dwarf_section_reloc *r, Elf_Data *data,
		  struct digested_reloc **digest,
		  GElf_Addr r_offset, GElf_Xword r_info, GElf_Sxword r_addend)
{
  const int r_type = GELF_R_TYPE (r_info);
  int symndx = GELF_R_SYM (r_info);

  size_t *nrel = &r->rel8.n;
  if (match_r_type (rel8_types, r_type))
    (*digest)->rel8 = true;
  else if (unlikely (!match_r_type (rel4_types, r_type)))
    return DWARF_E_RELBADTYPE;
  else if (unlikely ((GElf_Sxword) (GElf_Sword) r_addend != r_addend))
    return DWARF_E_RELBADADDEND;
  else
    {
      (*digest)->rel8 = false;
      nrel = &r->rel4.n;
    }

  if (unlikely (data->d_size - ((*digest)->rel8 ? 8 : 4) < r_offset))
    return DWARF_E_RELBADOFF;

  /* Fetch the symbol used in the reloc.  STN_UNDEF means no symbol,
     just an addend in some unallocated target DWARF section.  If the
     symbol is defined non-weak in another unallocated section, we can
     adjust the addend now and not bother to record any symbol.  */

  inline bool allocated_scn (GElf_Word shndx)
  {
    Elf_Scn *const symscn = ((Elf_Data_Scn *) r->symdata)->s;
    GElf_Shdr shdr;
    return (gelf_getshdr (elf_getscn (symscn->elf, shndx), &shdr) == NULL
	    || (shdr.sh_flags & SHF_ALLOC));
  }

  GElf_Sym sym;
  Elf32_Word shndx;
  if (symndx != STN_UNDEF
      && gelf_getsymshndx (r->symdata, r->symxndxdata, symndx, &sym, &shndx)
      && GELF_ST_BIND (sym.st_info) < STB_WEAK
      && (sym.st_shndx == SHN_XINDEX ? !allocated_scn (shndx)
	  : (sym.st_shndx != SHN_UNDEF
	     && sym.st_shndx < SHN_LORESERVE
	     && !allocated_scn (sym.st_shndx))))
    {
      r_addend += sym.st_value;
      symndx = STN_UNDEF;
    }

#if 0
  // XXX do in separate pass only for unstrip -R
  if (modify && symndx == STN_UNDEF)
    {
      /* This is fully resolved statically.
	 Modify it in place and record nothing.  */

      unsigned char *datum = (unsigned char *) (*digest)->datum;
      if ((*digest)->rel8)
	{
	  Elf64_Xword val = read_8ubyte_unaligned (dbg, datum) + r_addend;
	  if (dbg->other_byte_order)
	    val = bswap_64 (val);
	  memcpy (datum, &val, sizeof val);
	}
      else
	{
	  Elf32_Word val = read_4ubyte_unaligned (dbg, datum) + r_addend;
	  if (dbg->other_byte_order)
	    val = bswap_32 (val);
	  memcpy (datum, &val, sizeof val);
	}
      return 0;
    }
#endif

  if (r_addend != 0 || symndx != STN_UNDEF)
    {
      /* This will require relocating the data dynamically.  Record it.  */

      (*digest)->datum = data->d_buf + r_offset;
      (*digest)->symndx = symndx;
      (*digest)->addend = r_addend;
      ++*digest;
      ++*nrel;
    }

  return 0;
}

static int
compare_digested_reloc (const void *a, const void *b)
{
  const struct digested_reloc *r1 = a;
  const struct digested_reloc *r2 = b;
  return r1->datum > r2->datum ? 1 : r1->datum < r2->datum ? -1 : 0;
}

static int
digest_relocs (Dwarf *dbg, Elf_Data *data, struct dwarf_section_reloc *r)
{
  GElf_Shdr shdr;
  if (unlikely (gelf_getshdr (r->scn, &shdr) == NULL))
    assert (!"impossible gelf_getshdr failure");

  /* XXX let dwfl supply defaults from main file for separate debug
     with relocs pointing to SHT_NOBITS symtab
  r->symdata = dbg->relocate->symdata;
  r->symstrdata = dbg->relocate->symstrdata;
  */
  {
    GElf_Shdr symshdr;
    Elf_Scn *const symscn = elf_getscn (r->scn->elf, shdr.sh_link);
    if (unlikely (gelf_getshdr (symscn, &symshdr) == NULL))
      return DWARF_E_RELBADSYM;
    if (symshdr.sh_type != SHT_NOBITS)
      {
	r->symdata = elf_getdata (symscn, NULL);
	if (unlikely (r->symdata == NULL))
	  return DWARF_E_RELBADSYM;
	r->symstrdata = elf_getdata (elf_getscn (r->scn->elf, symshdr.sh_link),
				     NULL);
	if (unlikely (r->symstrdata == NULL))
	  return DWARF_E_RELBADSYM;
      }
  }

  if (dbg->relocate->ebl == NULL)
    {
      dbg->relocate->ebl = ebl_openbackend (dbg->elf);
      if (unlikely (dbg->relocate->ebl == NULL))
	return DWARF_E_NOMEM;
    }

  const int *rel8_types;
  const int *rel4_types;
  ebl_reloc_simple_types (dbg->relocate->ebl, &rel8_types, &rel4_types);

  Elf_Data *const reldata = elf_getdata (r->scn, NULL);

  const size_t nrel = shdr.sh_size / shdr.sh_entsize;
  struct digested_reloc digest[nrel];
  struct digested_reloc *d = digest;

  r->rel4.n = r->rel8.n = 0;

  int ret = 0;
  if (shdr.sh_type == SHT_RELA)
    for (size_t i = 0; i < nrel && !ret; ++i)
      {
	GElf_Rela rela;
	if (unlikely (gelf_getrela (reldata, i, &rela) == NULL))
	  assert (!"impossible gelf_getrela failure");
	ret = digest_one_reloc (rel8_types, rel4_types, r, data, &d,
				rela.r_offset, rela.r_info, rela.r_addend);
      }
  else
    for (size_t i = 0; i < nrel && !ret; ++i)
      {
	GElf_Rel rel;
	if (unlikely (gelf_getrel (reldata, i, &rel) == NULL))
	  assert (!"impossible gelf_getrel failure");
	ret = digest_one_reloc (rel8_types, rel4_types, r, data, &d,
				rel.r_offset, rel.r_info, 0);
      }

  assert (r->rel4.n + r->rel8.n == (size_t) (d - digest));

  if (ret)
    return ret;

  /* Sort by datum address.  */
  qsort (digest, d - digest, sizeof digest[0], &compare_digested_reloc);

  if (r->rel8.n > 0)
    {
      r->rel8.datum = libdw_alloc (dbg, const unsigned char *,
				   sizeof (const unsigned char *),
				   r->rel8.n);
      r->rel8.symndx = libdw_alloc (dbg, int, sizeof (int), r->rel8.n);
      if (shdr.sh_type == SHT_RELA)
	r->rela8 = libdw_alloc (dbg, Elf64_Sxword, sizeof (Elf64_Sxword),
				r->rel8.n);
      else
	r->rela8 = NULL;
    }

  if (r->rel4.n > 0)
    {
      r->rel4.datum = libdw_alloc (dbg, const unsigned char *,
				   sizeof (const unsigned char *),
				   r->rel4.n);
      r->rel4.symndx = libdw_alloc (dbg, int, sizeof (int), r->rel4.n);
      if (shdr.sh_type == SHT_RELA)
	r->rela4 = libdw_alloc (dbg, Elf32_Sword, sizeof (Elf32_Sword),
				r->rel4.n);
      else
	r->rela4 = NULL;
    }

  size_t n8 = 0;
  size_t n4 = 0;
  for (struct digested_reloc *dr = digest; dr < d; ++dr)
    if (dr->rel8)
      {
	r->rel8.datum[n8] = dr->datum;
	r->rel8.symndx[n8] = dr->symndx;
	if (shdr.sh_type == SHT_RELA)
	  r->rela8[n8] = dr->addend;
	++n8;
      }
    else
      {
	r->rel4.datum[n4] = dr->datum;
	r->rel4.symndx[n4] = dr->symndx;
	if (shdr.sh_type == SHT_RELA)
	  r->rela4[n4] = dr->addend;
	++n4;
      }
  assert (n8 == r->rel8.n);
  assert (n4 == r->rel4.n);

  /* Clearing this marks that the digested form is set up now.  */
  r->scn = NULL;
  return 0;
}

/* Binary search for an exact match on the DATUM address.  */
static ssize_t
find_reloc (struct dwarf_reloc_table *table, const unsigned char *datum)
{
  size_t l = 0, u = table->n;

  if (u == 0)
    return -1;

  if (table->hint < u && table->datum[table->hint] <= datum)
    {
      l = table->hint;
      if (table->datum[l] == datum)
	return l;
      if (table->datum[l] < datum)
	++l;
    }

  while (l < u)
    {
      size_t i = (l + u) / 2;
      if (table->datum[i] < datum)
	l = i + 1;
      else if (table->datum[i] > datum)
	u = i;
      else
	return table->hint = i;
    }

  table->hint = l;
  return -1;
}

int
internal_function
__libdw_relocatable (Dwarf *dbg, int sec_index,
		     const unsigned char *datum, int width,
		     int *symndx, GElf_Sxword *addend)
{
  if (dbg->relocate == NULL)
    {
    noreloc:
      if (addend != NULL)
	*addend = (width == 8
		   ? read_8ubyte_unaligned (dbg, datum)
		   : read_4ubyte_unaligned (dbg, datum));
      return 0;
    }

  struct dwarf_section_reloc *const r = dbg->relocate->sectionrel[sec_index];
  if (r == NULL)
    goto noreloc;

  if (r->scn != NULL)
    {
      int result = digest_relocs (dbg, dbg->sectiondata[sec_index], r);
      if (unlikely (result != 0))
	{
	  __libdw_seterrno (result);
	  return -1;
	}
    }

  ssize_t i;
  if (width == 4)
    {
      i = find_reloc (&r->rel4, datum);
      if (symndx != NULL)
	*symndx = i < 0 ? STN_UNDEF : r->rel4.symndx[i];
      if (addend != NULL)
	*addend = ((i < 0 || r->rela4 == NULL)
		   ? read_4sbyte_unaligned (dbg, datum) : r->rela4[i]);
    }
  else
    {
      assert (width == 8);

      i = find_reloc (&r->rel8, datum);
      if (symndx != NULL)
	*symndx = i < 0 ? STN_UNDEF : r->rel8.symndx[i];
      if (addend != NULL)
	*addend = ((i < 0 || r->rela8 == NULL)
		   ? read_8sbyte_unaligned (dbg, datum) : r->rela8[i]);
    }

  return i >= 0;
}

static int
reloc_getsym (struct dwarf_section_reloc *r, int symndx,
	      GElf_Sym *sym, GElf_Word *shndx)
{
  if (unlikely (gelf_getsymshndx (r->symdata, r->symxndxdata,
				  symndx, sym, shndx) == NULL))
    {
      __libdw_seterrno (DWARF_E_RELBADSYM);
      return -1;
    }
  return 1;
}

int
internal_function
__libdw_relocatable_getsym (Dwarf *dbg, int sec_index,
			    const unsigned char *datum, int width,
			    int *symndx, GElf_Sym *sym, GElf_Word *shndx,
			    GElf_Sxword *addend)
{
  int result = __libdw_relocatable (dbg, sec_index, datum, width,
				    symndx, addend);
  if (result > 0 && *symndx != STN_UNDEF)
    result = reloc_getsym (dbg->relocate->sectionrel[sec_index],
			   *symndx, sym, shndx);
  return result;
}

int
internal_function
__libdw_relocate_shndx (Dwarf *dbg, GElf_Word shndx, GElf_Sxword addend,
			Dwarf_Addr *val)
{
  GElf_Sym sym =
    {
      .st_shndx = shndx < SHN_LORESERVE ? shndx : SHN_XINDEX,
      .st_info = GELF_ST_INFO (STB_LOCAL, STT_SECTION),
    };
  int result = (*dbg->relocate->resolve_symbol) (false, dbg, &sym, shndx);
  if (result == 0)
    *val = sym.st_value + addend;
  return result;
}

int
internal_function
__libdw_relocate_address (Dwarf *dbg, int sec_index,
			  const void *datum, int width, Dwarf_Addr *val)
{
  int symndx;
  GElf_Sym sym;
  GElf_Word shndx;
  GElf_Sxword addend;
  int result = __libdw_relocatable_getsym (dbg, sec_index, datum, width,
					   &symndx, &sym, &shndx, &addend);
  if (result > 0 && symndx != STN_UNDEF)
    {
      result = (*dbg->relocate->resolve_symbol)
	(sym.st_shndx == SHN_UNDEF || sym.st_shndx == SHN_COMMON
	 || GELF_ST_BIND (sym.st_info) > STB_WEAK,
	 dbg, &sym, sym.st_shndx == SHN_XINDEX ? shndx : sym.st_shndx);
      addend += sym.st_value;
    }
  if (result >= 0)
    *val = addend;
  return result;
}

int
internal_function
__libdw_relocate_offset (Dwarf *dbg, int sec_index,
			 const void *datum, int width, Dwarf_Off *val)
{
  int symndx;
  GElf_Sym sym;
  GElf_Word shndx;
  GElf_Sxword addend;
  int result = __libdw_relocatable_getsym (dbg, sec_index, datum, width,
					   &symndx, &sym, &shndx, &addend);
  if (result > 0 && symndx != STN_UNDEF)
    {
      if (unlikely (sym.st_shndx == SHN_UNDEF)
	  || (sym.st_shndx > SHN_LORESERVE
	      && unlikely (sym.st_shndx != SHN_XINDEX)))
	{
	  __libdw_seterrno (DWARF_E_RELUNDEF);
	  return -1;
	}
      if (unlikely (((Elf_Data_Scn *) dbg->sectiondata[sec_index])->s->index
		    != (sym.st_shndx == SHN_XINDEX ? shndx : sym.st_shndx)))
	{
	  __libdw_seterrno (DWARF_E_RELWRONGSEC);
	  return -1;
	}
      addend += sym.st_value;
    }
  if (result >= 0)
    *val = addend;
  return result;
}
