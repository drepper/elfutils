/* Pedantic checking of DWARF files
   Copyright (C) 2009 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "dwarflint-checks-low.hh"
#include "dwarflint-low.h"
#include <map>
#include <sstream>
#include <cstring>

struct secentry
{
  Elf_Data *reldata;	/* Relocation data if any found.  */
  size_t reltype;	/* SHT_REL or SHT_RELA.  We need this
			   temporary store to be able to resolve
			   relocation section appearing before
			   relocated section.  */
  size_t secndx;	/* Index into file->sec or 0 if not yet loaded.  */
  section_id id;	/* Section type.  */

  explicit secentry (section_id a_id = sec_invalid)
    : reldata (NULL)
    , reltype (0)
    , secndx (0)
    , id (a_id)
  {}
};

struct secinfo_map
  : public std::map <std::string, secentry>
{
  secentry *get (const char *name)
  {
    iterator it = find (std::string (name));
    if (it == end ())
      return NULL;
    else
      return &it->second;
  }
};

bool
elf_file_init (struct elf_file *file, Elf *elf)
{
  memset (file, 0, sizeof (*file));

  file->elf = elf;
  file->ebl = ebl_openbackend (elf);

  if (file->ebl == NULL
      || gelf_getehdr (elf, &file->ehdr) == NULL)
    return false;

  file->addr_64 = file->ehdr.e_ident[EI_CLASS] == ELFCLASS64;

  /* Taken from dwarf_begin_elf.c.  */
  if ((BYTE_ORDER == LITTLE_ENDIAN
       && file->ehdr.e_ident[EI_DATA] == ELFDATA2MSB)
      || (BYTE_ORDER == BIG_ENDIAN
	  && file->ehdr.e_ident[EI_DATA] == ELFDATA2LSB))
    file->other_byte_order = true;

  Elf_Scn *reloc_symtab = NULL;

  secinfo_map secinfo;
#define SEC(n) secinfo[".debug_" #n] = secentry (sec_##n);
    DEBUGINFO_SECTIONS
#undef SEC

  /* Now find all necessary debuginfo sections and associated
     relocation sections.  */

  /* Section 0 is special, skip it.  */
  REALLOC (file, sec);
  file->sec[file->size++].id = sec_invalid;

  bool check_rel = true;

  for (Elf_Scn *scn = NULL; (scn = elf_nextscn (elf, scn)); )
    {
      REALLOC (file, sec);
      size_t curndx = file->size++;
      struct sec *cursec = file->sec + curndx;

      GElf_Shdr *shdr = gelf_getshdr (scn, &cursec->shdr);
      if (shdr == NULL)
	{
	invalid_elf:
	  wr_error (NULL, "Broken ELF.\n");
	  return false;
	}

      const char *scnname = elf_strptr (elf, file->ehdr.e_shstrndx,
					shdr->sh_name);
      if (scnname == NULL)
	goto invalid_elf;

      if (!address_aligned (shdr->sh_addr, shdr->sh_addralign))
	{
	  std::ostringstream s;
	  s << "Base address of section " << scnname << ", "
	    << "0x" << std::hex << shdr->sh_addr
	    << ", should have an alignment of "
	    << std::dec << shdr->sh_addralign;
	  wr_error (NULL, "%s\n", s.str ().c_str ());
	}

      secentry *entry = secinfo.get (scnname);
      cursec->scn = scn;
      cursec->id = entry != NULL ? entry->id : sec_invalid;
      cursec->name = scnname;
      cursec->rel = (struct relocation_data){NULL, SHT_NULL, NULL, 0, 0, 0};

      /* Dwarf section.  */
      if (entry != NULL)
	{
	  if (unlikely (entry->secndx != 0))
	    wr_error (NULL, "Multiple occurrences of section %s.\n", scnname);
	  else
	    {
	      /* Haven't seen a section of that name yet.  */
	      cursec->data = elf_getdata (scn, NULL);
	      if (cursec->data == NULL || cursec->data->d_buf == NULL)
		/* Don't print out a warning, we'll get to that in
		   process_file.  */
		cursec->data = NULL;
	      entry->secndx = curndx;
	    }
	}
      /* Relocation section.  */
      else if (shdr->sh_type == SHT_RELA || shdr->sh_type == SHT_REL)
	{
	  /* Get data of section that this REL(A) section relocates.  */
	  Elf_Scn *relocated_scn = elf_getscn (elf, shdr->sh_info);
	  Elf_Scn *symtab_scn = elf_getscn (elf, shdr->sh_link);
	  if (relocated_scn == NULL || symtab_scn == NULL)
	    goto invalid_elf;

	  GElf_Shdr relocated_shdr_mem;
	  GElf_Shdr *relocated_shdr = gelf_getshdr (relocated_scn,
						    &relocated_shdr_mem);
	  if (relocated_shdr == NULL)
	    goto invalid_elf;

	  const char *relocated_scnname
	    = elf_strptr (elf, file->ehdr.e_shstrndx,
			  relocated_shdr->sh_name);

	  secentry *relocated = secinfo.get (relocated_scnname);

	  if (relocated != NULL)
	    {
	      if (relocated->reldata != NULL)
		wr_error (NULL,
			  "Several relocation sections for debug section %s."
			  "  Ignoring %s.\n",
			  relocated_scnname, scnname);
	      else
		{
		  relocated->reldata = elf_getdata (scn, NULL);
		  if (unlikely (relocated->reldata == NULL
				|| relocated->reldata->d_buf == NULL))
		    {
		      wr_error (NULL,
				"Data-less relocation section %s.\n", scnname);
		      relocated->reldata = NULL;
		    }
		  else
		    relocated->reltype = shdr->sh_type;
		}

	      if (reloc_symtab == NULL)
		reloc_symtab = symtab_scn;
	      else if (reloc_symtab != symtab_scn)
		wr_error (NULL,
			  "Relocation sections use multiple symbol tables.\n");
	    }
	}
    }

  for (secinfo_map::iterator it = secinfo.begin (); it != secinfo.end (); ++it)
    if (it->second.secndx != 0)
      file->debugsec[it->second.id] = file->sec + it->second.secndx;

  if (check_rel)
    {
      Elf_Data *reloc_symdata = NULL;
      if (reloc_symtab != NULL)
	{
	  reloc_symdata = elf_getdata (reloc_symtab, NULL);
	  if (reloc_symdata == NULL)
	    /* Not a show stopper, we can check a lot of stuff even
	       without a symbol table.  */
	      wr_error (NULL,
			"Couldn't obtain symtab data.\n");
	}

      /* Check relocation sections that we've got.  */
      for (secinfo_map::iterator it = secinfo.begin (); it != secinfo.end (); ++it)
	{
	  secentry *cur = &it->second;
	  if (cur->secndx != 0 && cur->reldata != NULL)
	    {
	      struct sec *sec = file->sec + cur->secndx;
	      sec->rel.type = cur->reltype;
	      if (sec->data == NULL)
		{
		  where wh = WHERE (sec->id, NULL);
		  wr_error (&wh,
			    ": this data-less section has a relocation section.\n");
		}
	      else if (read_rel (file, sec, cur->reldata, file->addr_64))
		sec->rel.symdata = reloc_symdata;
	    }
	}

      if (secentry *str = secinfo.get (".debug_str"))
	if (str->reldata != NULL)
	  {
	    where wh = WHERE (sec_str, NULL);
	    wr_message (mc_impact_2 | mc_elf, &wh,
			": there's a relocation section associated with this section.\n");
	  }
    }

  return true;
}

load_sections::load_sections (dwarflint &lint)
{
  elf_file_init (&file, lint.elf ());
}

sec &
section_base::get_sec_or_throw (section_id secid)
{
  if (sec *s = sections->file.debugsec[secid])
    return *s;

  where wh = WHERE (secid, NULL);
  std::stringstream ss;
  ss << where_fmt (&wh) << ": data not found.";
  throw check_base::failed (ss.str ());
}

section_base::section_base (dwarflint &lint, section_id secid)
  : sections (lint.check (sections))
  , sect (get_sec_or_throw (secid))
  , file (sections->file)
{
}

check_debug_abbrev::check_debug_abbrev (dwarflint &lint)
  : _m_sec_abbr (lint.check (_m_sec_abbr))
{
  read_ctx ctx;
  read_ctx_init (&ctx, _m_sec_abbr->sect.data,
		 _m_sec_abbr->file.other_byte_order);

  /* xxx wrap C routine before proper loading is in place.  */
  abbrev_table *chain = abbrev_table_load (&ctx);
  if (chain == NULL)
    throw check_base::failed (""); // xxx
  for (abbrev_table *it = chain; it != NULL; it = it->next)
    abbrevs[it->offset] = *it;
  // abbrev_table_free (chain); xxx
  abbrev_chain = chain;
}

check_debug_info::check_debug_info (dwarflint &lint)
  : _m_sec_info (lint.check (_m_sec_info))
  , _m_sec_abbrev (lint.check (_m_sec_abbrev))
  , _m_sec_str (lint.check (_m_sec_str))
  , _m_abbrevs (lint.check (_m_abbrevs))
{
  memset (&cu_cov, 0, sizeof (cu_cov));

  /* xxx wrap C routine before proper loading is in place.  */
  cu *chain = check_info_structural
    (&_m_sec_info->file, &_m_sec_info->sect,
     _m_abbrevs->abbrev_chain, _m_sec_str->sect.data, &cu_cov);

  if (chain == NULL)
    throw check_base::failed (""); // xxx

  for (cu *cu = chain; cu != NULL; cu = cu->next)
    cus.push_back (*cu);
  // cu_free (chain); xxx
  cu_chain = chain;
}
