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

#include "low.h"
#include "config.h"
#include "pri.hh"
#include "check_debug_loc_range.hh"
#include "check_debug_info.hh"
#include <map>
#include <sstream>
#include <cstring>
#include <cassert>

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

namespace
{
int
layout_rel_file (Elf *elf)
{
  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    return 1;

  if (ehdr.e_type != ET_REL)
    return 0;

  /* Taken from libdwfl. */
  GElf_Addr base = 0;
  GElf_Addr start = 0, end = 0, bias = 0;

  bool first = true;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (unlikely (shdr == NULL))
	return 1;

      if (shdr->sh_flags & SHF_ALLOC)
	{
	  const GElf_Xword align = shdr->sh_addralign ?: 1;
	  const GElf_Addr next = (end + align - 1) & -align;
	  if (shdr->sh_addr == 0
	      /* Once we've started doing layout we have to do it all,
		 unless we just layed out the first section at 0 when
		 it already was at 0.  */
	      || (bias == 0 && end > start && end != next))
	    {
	      shdr->sh_addr = next;
	      if (end == base)
		/* This is the first section assigned a location.
		   Use its aligned address as the module's base.  */
		start = base = shdr->sh_addr;
	      else if (unlikely (base & (align - 1)))
		{
		  /* If BASE has less than the maximum alignment of
		     any section, we eat more than the optimal amount
		     of padding and so make the module's apparent
		     size come out larger than it would when placed
		     at zero.  So reset the layout with a better base.  */

		  start = end = base = (base + align - 1) & -align;
		  Elf_Scn *prev_scn = NULL;
		  do
		    {
		      prev_scn = elf_nextscn (elf, prev_scn);
		      GElf_Shdr prev_shdr_mem;
		      GElf_Shdr *prev_shdr = gelf_getshdr (prev_scn,
							   &prev_shdr_mem);
		      if (unlikely (prev_shdr == NULL))
			return 1;
		      if (prev_shdr->sh_flags & SHF_ALLOC)
			{
			  const GElf_Xword prev_align
			    = prev_shdr->sh_addralign ?: 1;

			  prev_shdr->sh_addr
			    = (end + prev_align - 1) & -prev_align;
			  end = prev_shdr->sh_addr + prev_shdr->sh_size;

			  if (unlikely (! gelf_update_shdr (prev_scn,
							    prev_shdr)))
			    return 1;
			}
		    }
		  while (prev_scn != scn);
		  continue;
		}

	      end = shdr->sh_addr + shdr->sh_size;
	      if (likely (shdr->sh_addr != 0)
		  && unlikely (! gelf_update_shdr (scn, shdr)))
		return 1;
	    }
	  else
	    {
	      /* The address is already assigned.  Just track it.  */
	      if (first || end < shdr->sh_addr + shdr->sh_size)
		end = shdr->sh_addr + shdr->sh_size;
	      if (first || bias > shdr->sh_addr)
		/* This is the lowest address in the module.  */
		bias = shdr->sh_addr;

	      if ((shdr->sh_addr - bias + base) & (align - 1))
		/* This section winds up misaligned using BASE.
		   Adjust BASE upwards to make it congruent to
		   the lowest section address in the file modulo ALIGN.  */
		base = (((base + align - 1) & -align)
			+ (bias & (align - 1)));
	    }

	  first = false;
	}
    }
  return 0;
}

  Elf *
  open_elf (int fd)
  {
    Elf *elf = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
    if (unlikely (elf == NULL))
      {
	wr_error ()
	  << "Error opening file: " << elf_errmsg (-1) << std::endl;
	throw check_base::failed ();
      }

    if (layout_rel_file (elf))
      {
	wr_error ()
	  << "Couldn't layout ET_REL file." << std::endl;
	throw check_base::failed ();
      }

    return elf;
  }

  bool
  elf_file_init (struct elf_file *file, int fd)
  {
    Elf *elf = open_elf (fd);
    assert (elf != NULL);
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
	    wr_error () << "Broken ELF." << std::endl;
	    goto close_and_out;
	  }

	const char *scnname = elf_strptr (elf, file->ehdr.e_shstrndx,
					  shdr->sh_name);
	if (scnname == NULL)
	  goto invalid_elf;

	if (!address_aligned (shdr->sh_addr, shdr->sh_addralign))
	  wr_error ()
	    << "Base address of section " << scnname << ", "
	    << pri::addr (shdr->sh_addr) << ", should have an alignment of "
	    << shdr->sh_addralign << std::endl;

	secentry *entry = secinfo.get (scnname);
	cursec->scn = scn;
	cursec->id = entry != NULL ? entry->id : sec_invalid;
	cursec->name = scnname;
	cursec->rel = (struct relocation_data){NULL, SHT_NULL, NULL, 0, 0, 0};

	/* Dwarf section.  */
	if (entry != NULL)
	  {
	    if (unlikely (entry->secndx != 0))
	      wr_error ()
		<< "Multiple occurrences of section " << scnname << std::endl;
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
		  wr_error ()
		    << "Several relocation sections for debug section "
		    << relocated_scnname << ". Ignoring " << scnname
		    << "." << std::endl;
		else
		  {
		    relocated->reldata = elf_getdata (scn, NULL);
		    if (unlikely (relocated->reldata == NULL
				  || relocated->reldata->d_buf == NULL))
		      {
			wr_error ()
			  << "Data-less relocation section " << scnname
			  << "." << std::endl;
			relocated->reldata = NULL;
		      }
		    else
		      relocated->reltype = shdr->sh_type;
		  }

		if (reloc_symtab == NULL)
		  reloc_symtab = symtab_scn;
		else if (reloc_symtab != symtab_scn)
		  wr_error ()
		    << "Relocation sections use multiple symbol tables."
		    << std::endl;
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
	      wr_error () << "Couldn't obtain symtab data." << std::endl;
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
		  wr_error (WHERE (sec->id, NULL))
		    << "this data-less section has a relocation section."
		    << std::endl;
		else if (read_rel (file, sec, cur->reldata, file->addr_64))
		  sec->rel.symdata = reloc_symdata;
	      }
	  }

	if (secentry *str = secinfo.get (".debug_str"))
	  if (str->reldata != NULL)
	    wr_message (WHERE (sec_str, NULL), cat (mc_impact_2, mc_elf))
	      << "there's a relocation section associated with this section."
	      << std::endl;
      }

    return true;

  close_and_out:
    if (elf != NULL)
      {
	elf_errno (); // clear errno
	elf_end (elf);
	int err = elf_errno ();
	if (err != 0)
	  wr_error ()
	    << "error while closing Elf descriptor: "
	    << elf_errmsg (err) << std::endl;
      }
    return false;
  }
}

load_sections::load_sections (dwarflint &lint)
{
  if (!elf_file_init (&file, lint.fd ()))
    throw check_base::failed ();
}

load_sections::~load_sections ()
{
  if (file.ebl != NULL)
    ebl_closebackend (file.ebl);
  free (file.sec);
  elf_end (file.elf);
}

namespace
{
  message_category
  secid_to_cat (section_id secid)
  {
    switch (secid)
      {
      case sec_info: return mc_info;
      case sec_abbrev: return mc_abbrevs;
      case sec_aranges: return mc_aranges;
      case sec_str: return mc_strings;
      case sec_line: return mc_line;
      case sec_loc: return mc_loc;
      case sec_ranges: return mc_ranges;

      case sec_pubnames:
      case sec_pubtypes:
	return mc_pubtables;

      case sec_rel:
      case sec_rela:
	return mc_reloc;

	// xxx don't have one
      case sec_mac:
      case sec_invalid:
      case sec_locexpr:
      case rel_value:
      case rel_address:
      case rel_exec:
	break;
      };
    std::stringstream ss;
    ss << "Couldn't convert secid " << secid << " to mc.";
    throw std::runtime_error (ss.str ());
  }
}
sec &
section_base::get_sec_or_throw (section_id secid)
{
  if (sec *s = sections->file.debugsec[secid])
    return *s;

  if (!tolerate_nodebug)
    wr_message (WHERE (secid, NULL),
		cat (mc_impact_4, mc_acc_suboptimal, mc_elf,
		     secid_to_cat (secid)))
      << "data not found." << std::endl;

  throw check_base::failed ();
}

section_base::section_base (dwarflint &lint, section_id secid)
  : sections (lint.check (sections))
  , sect (get_sec_or_throw (secid))
  , file (sections->file)
{
}

check_debug_aranges::check_debug_aranges (dwarflint &lint)
  : _m_sec_aranges (lint.check (_m_sec_aranges))
{
  check_debug_info *info = lint.toplev_check<check_debug_info> ();
  coverage *cov = NULL;
  if (info != NULL)
    {
      // xxx If need_ranges is true, we have to load ranges first.
      // That's a flaw in design of checks, that data should have been
      // stored in check_ranges, and that should have been requested
      // explicitly.  But for the time being...
      if (info->cu_cov.need_ranges)
	lint.toplev_check<check_debug_ranges> ();
      if (!info->cu_cov.need_ranges)
	cov = &info->cu_cov.cov;
    }

  if (!check_aranges_structural (&_m_sec_aranges->file,
				 &_m_sec_aranges->sect,
				 info != NULL ? &info->cus.front () : NULL,
				 cov))
    throw check_base::failed ();
}

namespace
{
  template<section_id sec_id>
  class check_debug_pub
    : public check<check_debug_pub<sec_id> >
  {
    section<sec_id> *_m_sec;
    check_debug_info *_m_cus;

  public:
    explicit check_debug_pub (dwarflint &lint)
      : _m_sec (lint.check (_m_sec))
      , _m_cus (lint.check (_m_cus))
    {
      if (!check_pub_structural (&_m_sec->file,
				 &_m_sec->sect,
				 &_m_cus->cus.front ()))
	throw check_base::failed ();
    }
  };

  reg<check_debug_pub<sec_pubnames> > reg_debug_pubnames;
  reg<check_debug_pub<sec_pubtypes> > reg_debug_pubtypes;
}
