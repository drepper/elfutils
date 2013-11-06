/* Provide virtual symbols for ppc64 function descriptors.
   Copyright (C) 2013 Red Hat, Inc.
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

#include <assert.h>
#include <endian.h>
#include <string.h>
#include <stdlib.h>

#define BACKEND ppc64_
#include "libebl_CPU.h"

/* Pointer to the first entry is stored into ebl->backend.
   It has Dwfl_Module->ebl_syments entries and its memory is followed by
   strings for the NAME entries.  */

struct sym_entry
{
  GElf_Sym sym;
  GElf_Word shndx;
  const char *name;
};

/* Find section containing ADDR in its address range.  */

static Elf_Scn *
scnfindaddr (Elf *elf, GElf_Addr addr)
{
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (likely (shdr != NULL)
	  && likely ((shdr->sh_flags & SHF_ALLOC) != 0)
	  && addr >= shdr->sh_addr
	  && addr < shdr->sh_addr + shdr->sh_size)
	break;
    }
  return scn;
}

static int
symnames_get_compar (const void *ap, const void *bp)
{
  const char *const *a = ap;
  const char *const *b = bp;
  return strcmp (*a, *b);
}

static const char **
symnames_get (size_t syments, ebl_getsym_t *getsym, void *arg, bool is_linux,
	      size_t *symnames_countp)
{
  assert (syments > 0);
  const char **symnames = malloc (syments * sizeof (*symnames));
  if (symnames == NULL)
    return NULL;
  *symnames_countp = 0;
  for (size_t symi = 0; symi < syments; symi++)
    {
      GElf_Sym sym;
      const char *symname = getsym (arg, symi, &sym, NULL, NULL);
      if (symname == NULL || symname[0] != '.')
	continue;
      if (GELF_ST_TYPE (sym.st_info) != STT_FUNC
	  && (! is_linux || GELF_ST_TYPE (sym.st_info) != STT_GNU_IFUNC))
	continue;
      symnames[(*symnames_countp)++] = &symname[1];
    }
  qsort (symnames, *symnames_countp, sizeof (*symnames), symnames_get_compar);
  return symnames;
}

/* Scan all the symbols of EBL and create table of struct sym_entry entries for
   every function symbol pointing to the .opd section.  This includes automatic
   derefencing of the symbols via the .opd section content to create the
   virtual symbols for the struct sym_entry entries.  As GETSYM points to
   dwfl_module_getsym we scan all the available symbols, either
   Dwfl_Module.main plus Dwfl_Module.aux_sym or Dwfl_Module.debug.
   Symbols from all the available files are included in the SYMENTS count.

   .opd section contents is described in:
   http://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi-1.9.html#FUNC-DES
   http://www.ibm.com/developerworks/library/l-ppc/
    - see: Function descriptors -- the .opd section  */

bool
ppc64_init_symbols (Ebl *ebl, size_t syments, GElf_Addr main_bias,
		    ebl_getsym_t *getsym, void *arg, size_t *ebl_symentsp,
		    int *ebl_first_globalp)
{
  assert (ebl != NULL);
  assert (ebl->backend == NULL);
  if (syments == 0)
    return true;
  Elf *elf = ebl->elf;
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return false;
  GElf_Shdr opd_shdr_mem, *opd_shdr;
  Elf_Data *opd_data = NULL;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      opd_shdr = gelf_getshdr (scn, &opd_shdr_mem);
      if (opd_shdr == NULL || (opd_shdr->sh_flags & SHF_ALLOC) == 0)
	continue;
      if (strcmp (elf_strptr (elf, ehdr->e_shstrndx, opd_shdr->sh_name), ".opd")
	  != 0)
	continue;
      opd_data = elf_getdata (scn, NULL);
      /* SHT_NOBITS will produce NULL D_BUF.  */
      if (opd_data == NULL || opd_data->d_buf == NULL)
	return false;
      assert (opd_data->d_size == opd_shdr->sh_size);
      break;
    }
  if (opd_data == NULL)
    return true;
  char *ident = elf_getident (elf, NULL);
  bool is_linux = ident != NULL && ident[EI_OSABI] == ELFOSABI_LINUX;
  size_t symnames_count;
  const char **symnames = symnames_get (syments, getsym, arg, is_linux,
					&symnames_count);
  if (symnames == NULL)
    return false;
  struct sym_entry *sym_table = malloc (syments * sizeof (*sym_table));
  if (sym_table == NULL)
    {
      free (symnames);
      return false;
    }
  ebl->backend = sym_table;
  size_t names_size = 0;
  size_t ebl_syments = 0;
  int ebl_first_global = 0;
  for (size_t symi = 0; symi < syments; symi++)
    {
      GElf_Sym sym;
      GElf_Word sym_shndx;
      Elf *sym_elf;
      const char *symname = getsym (arg, symi, &sym, &sym_shndx, &sym_elf);
      if (symname == NULL || symname[0] == '.')
	continue;
      if (GELF_ST_TYPE (sym.st_info) != STT_FUNC
	  && (! is_linux || GELF_ST_TYPE (sym.st_info) != STT_GNU_IFUNC))
	continue;
      Elf_Scn *sym_scn = elf_getscn (sym_elf, sym_shndx);
      if (sym_scn == NULL)
	continue;
      GElf_Shdr sym_shdr_mem, *sym_shdr;
      sym_shdr = gelf_getshdr (sym_scn, &sym_shdr_mem);
      if (sym_shdr == NULL)
	continue;
      GElf_Ehdr sym_ehdr_mem, *sym_ehdr = gelf_getehdr (sym_elf, &sym_ehdr_mem);
      if (sym_ehdr == NULL)
	continue;
      if (strcmp (elf_strptr (sym_elf, sym_ehdr->e_shstrndx, sym_shdr->sh_name),
		  ".opd")
	  != 0)
	continue;
      if (sym.st_value < opd_shdr->sh_addr + main_bias
          || sym.st_value > (opd_shdr->sh_addr + main_bias
			     + opd_shdr->sh_size - sizeof (uint64_t)))
	continue;
      if (bsearch (&symname, symnames, symnames_count, sizeof (*symnames),
		   symnames_get_compar)
	  != 0)
	continue;
      uint64_t val64 = *(const uint64_t *) (opd_data->d_buf + sym.st_value
					    - (opd_shdr->sh_addr + main_bias));
      val64 = (elf_getident (elf, NULL)[EI_DATA] == ELFDATA2MSB
	       ? be64toh (val64) : le64toh (val64));
      Elf_Scn *entry_scn = scnfindaddr (elf, val64);
      if (entry_scn == NULL)
	continue;
      if (GELF_ST_BIND (sym.st_info) == STB_LOCAL)
	ebl_first_global++;
      struct sym_entry *dest = &sym_table[ebl_syments];
      dest->sym = sym;
      dest->sym.st_value = val64 + main_bias;
      dest->shndx = elf_ndxscn (entry_scn);
      dest->sym.st_shndx = (dest->shndx > SHN_HIRESERVE
			    ? SHN_XINDEX : dest->shndx);
      dest->name = symname;
      names_size += 1 + strlen (symname) + 1;
      ebl_syments++;
    }
  free (symnames);
  if (ebl_syments == 0)
    {
      free (sym_table);
      ebl->backend = NULL;
      return true;
    }
  sym_table = realloc (sym_table,
		       ebl_syments * sizeof (*sym_table) + names_size);
  if (sym_table == NULL)
    return false;
  ebl->backend = sym_table;
  char *names = (void *) &sym_table[ebl_syments];
  char *names_dest = names;
  for (size_t symi = 0; symi < ebl_syments; symi++)
    {
      struct sym_entry *dest = &sym_table[symi];
      const char *symname = dest->name;
      dest->name = names_dest;
      *names_dest++ = '.';
      names_dest = stpcpy (names_dest, symname) + 1;
    }
  assert (names_dest == names + names_size);
  *ebl_symentsp = ebl_syments;
  *ebl_first_globalp = ebl_first_global;
  return true;
}

const char *
ppc64_get_symbol (Ebl *ebl, size_t ndx, GElf_Sym *symp, GElf_Word *shndxp)
{
  assert (ebl != NULL);
  if (ebl->backend == NULL)
    return NULL;
  struct sym_entry *sym_table = ebl->backend;
  const struct sym_entry *found = &sym_table[ndx];
  *symp = found->sym;
  if (shndxp)
    *shndxp = found->shndx;
  return found->name;
}

void
ppc64_destr (Ebl *ebl)
{
  free (ebl->backend);
}
