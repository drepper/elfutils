/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2005, 2006, 2007 Red Hat, Inc.
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

#include "libdwflP.h"

/* This function is called when load_symtab finds necessary sections
   in ELF image.  It sets up FILE data apropriately, signalling error
   when one of the calls fails. */
static Dwfl_Error
load_symtab_sections (struct dwfl_shared_file *file, Elf_Scn *symscn,
		      Elf_Scn *xndxscn, GElf_Word strshndx)
{
  /* This does some sanity checks on the string table section.  */
  if (elf_strptr (file->elf, strshndx, 0) == NULL)
    goto elferr;

  if (xndxscn == NULL)
    file->symxndxdata = NULL;
  else
    {
      file->symxndxdata = elf_getdata (xndxscn, NULL);
      if (file->symxndxdata == NULL)
        goto elferr;
    }

  file->symdata = elf_getdata (symscn, NULL);
  if (file->symdata != NULL)
    return DWFL_E_NOERROR;

 elferr:
  return file->symerr = DWFL_E (LIBELF, elf_errno ());
}

/* Try to find a symbol table in FILE.
   Returns DWFL_E_NOERROR if a proper one is found.
   Returns DWFL_E_NO_SYMTAB if not, but still sets results for SHT_DYNSYM.  */
static Dwfl_Error
find_symtab (struct dwfl_shared_file *file, GElf_Word *strshndx)
{
  bool symtab = false;
  Elf_Scn *symscn = NULL, *xndxscn = NULL;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (file->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem, *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr != NULL)
	switch (shdr->sh_type)
	  {
	  case SHT_SYMTAB:
	    symtab = true;
	    symscn = scn;
	    *strshndx = shdr->sh_link;
	    file->syments = shdr->sh_size / shdr->sh_entsize;
	    if (xndxscn != NULL)
	      return load_symtab_sections (file, symscn, xndxscn, *strshndx);
	    break;

	  case SHT_DYNSYM:
	    if (symtab)
	      break;
	    symscn = scn;
	    *strshndx = shdr->sh_link;
	    file->syments = shdr->sh_size / shdr->sh_entsize;
	    break;

	  case SHT_SYMTAB_SHNDX:
	    xndxscn = scn;
	    if (symtab)
	      return load_symtab_sections (file, symscn, xndxscn, *strshndx);
	    break;

	  default:
	    break;
	  }
    }

  if (symtab)
    /* We found one, though no SHT_SYMTAB_SHNDX to go with it.  */
    return load_symtab_sections (file, symscn, xndxscn, *strshndx);

 /* We found no SHT_SYMTAB.  */
  return DWFL_E_NO_SYMTAB;
}

/* Translate addresses into file offsets.
   OFFS[*] start out zero and remain zero if unresolved.  */
static void
find_offsets (Elf *elf, const GElf_Ehdr *ehdr, size_t n,
	      GElf_Addr addrs[n], GElf_Off offs[n])
{
  size_t unsolved = n;
  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (phdr != NULL && phdr->p_type == PT_LOAD && phdr->p_memsz > 0)
	for (size_t j = 0; j < n; ++j)
	  if (offs[j] == 0
	      && addrs[j] >= phdr->p_vaddr
	      && addrs[j] - phdr->p_vaddr < phdr->p_filesz)
	    {
	      offs[j] = addrs[j] - phdr->p_vaddr + phdr->p_offset;
	      if (--unsolved == 0)
		break;
	    }
    }
}

/* Try to find a dynamic symbol table via phdrs.  */
static Dwfl_Error
find_dynsym (struct dwfl_shared_file *file)
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (file->elf, &ehdr_mem);

  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (file->elf, i, &phdr_mem);
      if (phdr == NULL)
	break;

      if (phdr->p_type == PT_DYNAMIC)
	{
	  /* Examine the dynamic section for the pointers we need.  */

	  Elf_Data *data = elf_getdata_rawchunk (file->elf,
						 phdr->p_offset, phdr->p_filesz,
						 ELF_T_DYN);
	  if (data == NULL)
	    continue;

	  enum
	    {
	      i_symtab,
	      i_strtab,
	      i_hash,
	      i_gnu_hash,
	      i_max
	    };
	  GElf_Addr addrs[i_max] = { 0, };
	  GElf_Xword strsz = 0;
	  size_t n = data->d_size / gelf_fsize (file->elf,
						ELF_T_DYN, 1, EV_CURRENT);
	  for (size_t j = 0; j < n; ++j)
	    {
	      GElf_Dyn dyn_mem;
	      GElf_Dyn *dyn = gelf_getdyn (data, j, &dyn_mem);
	      if (dyn != NULL)
		switch (dyn->d_tag)
		  {
		  case DT_SYMTAB:
		    addrs[i_symtab] = dyn->d_un.d_ptr;
		    continue;

		  case DT_HASH:
		    addrs[i_hash] = dyn->d_un.d_ptr;
		    continue;

		  case DT_GNU_HASH:
		    addrs[i_gnu_hash] = dyn->d_un.d_ptr;
		    continue;

		  case DT_STRTAB:
		    addrs[i_strtab] = dyn->d_un.d_ptr;
		    continue;

		  case DT_STRSZ:
		    strsz = dyn->d_un.d_val;
		    continue;

		  default:
		    continue;

		  case DT_NULL:
		    break;
		  }
	      break;
	    }

	  /* Translate pointers into file offsets.  */
	  GElf_Off offs[i_max] = { 0, };
	  find_offsets (file->elf, ehdr, i_max, addrs, offs);

	  /* Figure out the size of the symbol table.  */
	  if (offs[i_hash] != 0)
	    {
	      /* In the original format, .hash says the size of .dynsym.  */

	      size_t entsz = SH_ENTSIZE_HASH (ehdr);
	      data = elf_getdata_rawchunk (file->elf,
					   offs[i_hash] + entsz, entsz,
					   entsz == 4 ? ELF_T_WORD
					   : ELF_T_XWORD);
	      if (data != NULL)
		file->syments = (entsz == 4
				 ? *(const GElf_Word *) data->d_buf
				 : *(const GElf_Xword *) data->d_buf);
	    }
	  if (offs[i_gnu_hash] != 0 && file->syments == 0)
	    {
	      /* In the new format, we can derive it with some work.  */

	      const struct
	      {
		Elf32_Word nbuckets;
		Elf32_Word symndx;
		Elf32_Word maskwords;
		Elf32_Word shift2;
	      } *header;

	      data = elf_getdata_rawchunk (file->elf, offs[i_gnu_hash],
					   sizeof *header, ELF_T_WORD);
	      if (data != NULL)
		{
		  header = data->d_buf;
		  Elf32_Word nbuckets = header->nbuckets;
		  Elf32_Word symndx = header->symndx;
		  GElf_Off buckets_at = (offs[i_gnu_hash] + sizeof *header
					 + (gelf_getclass (file->elf)
					    * sizeof (Elf32_Word)
					    * header->maskwords));

		  data = elf_getdata_rawchunk (file->elf, buckets_at,
					       nbuckets * sizeof (Elf32_Word),
					       ELF_T_WORD);
		  if (data != NULL && symndx < nbuckets)
		    {
		      const Elf32_Word *const buckets = data->d_buf;
		      Elf32_Word maxndx = symndx;
		      for (Elf32_Word bucket = 0; bucket < nbuckets; ++bucket)
			if (buckets[bucket] > maxndx)
			  maxndx = buckets[bucket];

		      GElf_Off hasharr_at = (buckets_at
					     + nbuckets * sizeof (Elf32_Word));
		      hasharr_at += (maxndx - symndx) * sizeof (Elf32_Word);
		      do
			{
			  data = elf_getdata_rawchunk (file->elf,
						       hasharr_at,
						       sizeof (Elf32_Word),
						       ELF_T_WORD);
			  if (data != NULL
			      && (*(const Elf32_Word *) data->d_buf & 1u))
			    {
			      file->syments = maxndx + 1;
			      break;
			    }
			  ++maxndx;
			  hasharr_at += sizeof (Elf32_Word);
			} while (data != NULL);
		    }
		}
	    }
	  if (offs[i_strtab] > offs[i_symtab] && file->syments == 0)
	    file->syments = ((offs[i_strtab] - offs[i_symtab])
				     / gelf_fsize (file->elf,
						   ELF_T_SYM, 1, EV_CURRENT));

	  if (file->syments > 0)
	    {
	      file->symdata
		= elf_getdata_rawchunk (file->elf,
					offs[i_symtab],
					gelf_fsize (file->elf,
						    ELF_T_SYM,
						    file->syments,
						    EV_CURRENT),
					ELF_T_SYM);
	      if (file->symdata != NULL)
		{
		  file->symstrdata
		    = elf_getdata_rawchunk (file->elf,
					    offs[i_strtab],
					    strsz,
					    ELF_T_BYTE);
		  if (file->symstrdata == NULL)
		    file->symdata = NULL;
		}
	      if (file->symdata == NULL)
		return DWFL_E (LIBELF, elf_errno ());
	      else
		return DWFL_E_NOERROR;
	    }
	}
    }
  return DWFL_E_NO_SYMTAB;
}

/* Try to find a symbol table or dynamic symbol table in FILE.  */
Dwfl_Error
internal_function
__libdwfl_find_symtab (struct dwfl_shared_file *file)
{
  if (file->symdata != NULL /* Already done.  */
      || file->symerr != DWFL_E_NOERROR) /* Cached previous failure.  */
    return file->symerr;

  /* First see if .symtab is present.  */
  GElf_Word strshndx = 0;
  file->symerr = find_symtab (file, &strshndx);

  if (file->symerr == DWFL_E_NOERROR)
    file->is_symtab = true;
  else if (file->symerr == DWFL_E_NO_SYMTAB)
    {
      /* .symtab not present, try .dynsym */
      file->symerr = find_dynsym (file);

      if (file->symerr != DWFL_E_NOERROR)
	return file->symerr;
    }
  else
    return file->symerr;

  file->symstrdata = elf_getdata (elf_getscn (file->elf, strshndx), NULL);
  if (file->symstrdata == NULL)
    file->symerr = DWFL_E (LIBELF, elf_errno ());
  else
    file->symerr = DWFL_E_NOERROR;

  return file->symerr;
}
