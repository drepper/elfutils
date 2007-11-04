/* Examine a core file to guess the modules used in the crashed process.
   Copyright (C) 2007 Red Hat, Inc.
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

#include <config.h>
#include "libdwflP.h"

#include <gelf.h>
#include <inttypes.h>
#include <alloca.h>

#define MIN(a, b)  ((a) < (b) ? (a) : (b))


/* Determine whether a module already reported in this reporting phase
   overlaps START..END.  If REORDER, move it to the end of the list
   as dwfl_report_module on an existing module would do.  */
static Dwfl_Module *
module_overlapping (Dwfl *dwfl, GElf_Addr start, GElf_Addr end, bool reorder)
{
  Dwfl_Module **tailp = &dwfl->modulelist, **prevp = tailp;
  for (Dwfl_Module *mod = *prevp; mod != NULL; mod = *(prevp = &mod->next))
    if (! mod->gc)
      {
	if ((start >= mod->low_addr && start < mod->high_addr)
	    || (end >= mod->low_addr && end < mod->high_addr)
	    || (mod->low_addr >= start && mod->low_addr < end))
	  {
	    if (reorder)
	      {
		*prevp = mod->next;
		mod->next = *tailp;
		*tailp = mod;
	      }
	    return mod;
	  }

	tailp = &mod->next;
      }

  return NULL;
}


/* Collected ideas about each module, stored in Dwfl_Module.cb_data.  */
struct core_module_info
{
  GElf_Off offset;		/* Start position in the core file.  */
  GElf_Word whole_size;		/* Zero or size of whole image at offset.  */
  GElf_Addr l_name_vaddr;	/* l_name file name from dynamic linker.  */
};


static GElf_Off
offset_from_addr (Elf *elf, GElf_Addr addr, GElf_Off *limit)
{
  // XXX optimize with binary search of cached merged regions

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return 0;

  GElf_Addr end = 0;
  GElf_Off offset = 0;
  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (phdr == NULL)
	break;
      if (phdr->p_type != PT_LOAD || phdr->p_vaddr + phdr->p_memsz <= addr)
	continue;
      if (offset == 0)
	offset = addr - phdr->p_vaddr + phdr->p_offset;
      else if ((phdr->p_vaddr & -phdr->p_align) > end)
	break;
      end = phdr->p_vaddr + phdr->p_filesz;
    }

  *limit = end - addr + offset;
  return offset;
}

/* Do gelf_rawchunk to get a '\0'-terminated string at
   the given offset in the core file.  Ignore an empty string.  */
static char *
string_from_offset (Elf *core, GElf_Off string_offset, GElf_Off limit)
{
  GElf_Off offset = string_offset;

#define STRING_BUF_SIZE	64
  while (offset < limit)
    {
      const size_t sample_size = MIN (limit - offset, STRING_BUF_SIZE);
      char *sample = gelf_rawchunk (core, offset, sample_size);
      if (sample == NULL)
	break;
      char *end = memchr (sample, '\0', sample_size);
      if (end == NULL)
	{
	  gelf_freechunk (core, sample);
	  offset += sample_size;
	}
      else if (offset == string_offset)
	{
	  if (end != sample)
	    return sample;
	  /* It's the empty string.  */
	  gelf_freechunk (core, sample);
	  return NULL;
	}
      else
	{
	  gelf_freechunk (core, sample);
	  return gelf_rawchunk (core, string_offset,
				(offset - string_offset) + (end - sample));
	}
    }

  return NULL;
}

// XXX l_name can be in elided text (.interp), needs module-integrated read here
/* Do gelf_rawchunk to get a '\0'-terminated string at
   the given address in the core file memory image.  */
static char *
string_from_memory (Elf *core, GElf_Addr addr)
{
  GElf_Off limit;
  GElf_Off offset = offset_from_addr (core, addr, &limit);
  return offset == 0 ? NULL : string_from_offset (core, offset, limit);
}

/* Do gelf_getdata_rawchunk given an address in the core file memory image.  */
static Elf_Data *
getdata_core (Elf *core, GElf_Addr addr, GElf_Word size, Elf_Type type)
{
  GElf_Off limit;
  GElf_Off offset = offset_from_addr (core, addr, &limit);
  return (limit - offset < size ? NULL
	  : gelf_getdata_rawchunk (core, offset, size, type));
}

/* Fetch one address word from the core file memory image.  */
static GElf_Addr
addr_from_memory (Elf *core, GElf_Addr addr)
{
  const size_t addrsize = gelf_fsize (core, ELF_T_ADDR, 1, EV_CURRENT);
  Elf_Data *data = getdata_core (core, addr, addrsize, ELF_T_ADDR);
  if (data == NULL)
    return 0;
  return (addrsize == 4
	  ? *(const Elf32_Addr *) data->d_buf
	  : *(const Elf64_Addr *) data->d_buf);
}

/* Process a struct link_map extracted from the core file.
   Report a module if it describes one we can figure out.  */
static int
report_link_map (int result, Elf *core, Dwfl *dwfl,
		 GElf_Addr l_addr, GElf_Addr l_name, GElf_Addr l_ld)
{
  /* The l_ld address is a runtime address inside the module,
     so we can use that alone to see if we already know this module.
     This moves the one found to the end of the order as a side effect. */
  Dwfl_Module *mod = module_overlapping (dwfl, l_ld, l_ld + 1, true);

  if (mod == NULL)
    {
      /* We have to find the file's phdrs to compute along with l_addr
	 what its runtime address boundaries are.  */

      char *file_name = string_from_memory (core, l_name);
      if (file_name != NULL)
	{
	  mod = INTUSE(dwfl_report_elf) (dwfl, basename (file_name),
					 file_name, -1, l_addr);
	  if (mod != NULL)
	    result = 1;
	  gelf_freechunk (core, file_name);
	}

      return result;
    }

  if (mod->cb_data != NULL)
    {
      /* This is a module we recognized before from the core contents.  */
      struct core_module_info *info = mod->cb_data;
      info->l_name_vaddr = l_name;
      if (mod->name[0] == '[')
	{
	  /* We gave it a boring synthetic name.
	     Use the basename of its l_name string instead.  */
	  char *chunk = string_from_memory (core, info->l_name_vaddr);
	  if (chunk != NULL)
	    {
	      char *newname = strdup (basename (chunk));
	      if (newname != NULL)
		{
		  free (mod->name);
		  mod->name = newname;
		}
	      gelf_freechunk (core, chunk);
	    }
	}
    }

  return result;
}

/* Call report_link_map for each struct link_map in the linked list at r_map
   in the struct r_debug at R_DEBUG_VADDR.  */
static int
report_r_debug (int result, Elf *core, Dwfl *dwfl, GElf_Addr r_debug_vaddr)
{
  const size_t addrsize = gelf_fsize (core, ELF_T_ADDR, 1, EV_CURRENT);

  /* Skip r_version, to aligned r_map field.  */
  GElf_Addr next = addr_from_memory (core, r_debug_vaddr + addrsize);

  while (result >= 0 && next != 0)
    {
      Elf_Data *data = getdata_core (core, next, addrsize * 4, ELF_T_ADDR);
      if (unlikely (data == NULL))
	result = -1;
      else
	{
	  GElf_Addr addr;
	  GElf_Addr name;
	  GElf_Addr ld;

	  if (addrsize == 4)
	    {
	      const Elf32_Addr *map = data->d_buf;
	      addr = map[0];
	      name = map[1];
	      ld = map[2];
	      next = map[3];
	    }
	  else
	    {
	      const Elf64_Addr *map = data->d_buf;
	      addr = map[0];
	      name = map[1];
	      ld = map[2];
	      next = map[3];
	    }

	  result = report_link_map (result, core, dwfl, addr, name, ld);
	}
    }

  return result;
}

/* Find the vaddr of the DT_DEBUG's d_ptr.  This is the memory address
   where &r_debug was written at runtime.  */
static GElf_Addr
find_dt_debug (Elf *elf, GElf_Addr bias)
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return 0;

  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (phdr == NULL)
	break;
      if (phdr->p_type == PT_DYNAMIC)
	{
	  Elf_Data *data = gelf_getdata_rawchunk (elf, phdr->p_offset,
						  phdr->p_filesz, ELF_T_DYN);
	  if (data == NULL)
	    continue;
	  const size_t entsize = gelf_fsize (elf, ELF_T_DYN, 1, EV_CURRENT);
	  const size_t n = data->d_size / entsize;
	  for (size_t j = 0; j < n; ++j)
	    {
	      GElf_Dyn dyn_mem;
	      GElf_Dyn *dyn = gelf_getdyn (data, j, &dyn_mem);
	      if (dyn != NULL && dyn->d_tag == DT_DEBUG)
		return phdr->p_vaddr + bias + entsize * j + entsize / 2;
	    }
	}
    }

  return 0;
}


int
dwfl_core_file_report (Dwfl *dwfl, Elf *core)
{
  if (dwfl == NULL)
    return -1;

  int result = 0;
  const size_t ehdr_size = gelf_fsize (core, ELF_T_EHDR, 1, EV_CURRENT);

  /* Record when we find a GNU build-ID note.  */
  GElf_Off build_id_offset;
  GElf_Word build_id_size;
  GElf_Addr build_id_vaddr;
  inline void handle_build_id (GElf_Off offset, GElf_Word size, GElf_Addr vaddr)
    {
      build_id_offset = offset;
      build_id_size = size;
      build_id_vaddr = vaddr;
    }

  /* We've found a PT_NOTE segment inside an ELF image.  Investigate.  */
  inline void handle_note (GElf_Off offset, GElf_Xword filesz, GElf_Addr vaddr)
    {
      Elf_Data *data = gelf_getdata_rawchunk (core, offset, filesz, ELF_T_NHDR);
      if (data == NULL)
	return;
      size_t pos = 0;
      GElf_Nhdr nhdr;
      size_t name_offset;
      size_t desc_offset;
      while (pos < data->d_size
	     && (pos = gelf_getnote (data, pos,
				     &nhdr, &name_offset, &desc_offset)) > 0)
	if (nhdr.n_type == NT_GNU_BUILD_ID
	    && nhdr.n_namesz == sizeof "GNU"
	    && !memcmp (data->d_buf + name_offset, "GNU", sizeof "GNU"))
	  handle_build_id (offset + desc_offset, nhdr.n_descsz,
			   vaddr + desc_offset);
    }

  /* We've found the PT_DYNAMIC segment inside an ELF image.
     Return the absolute vaddr of the SONAME string if we find one.  */
  inline GElf_Addr handle_dyn (GElf_Off offset, GElf_Word filesz,
 			       GElf_Addr loadbase, GElf_Addr *strtab_end,
			       GElf_Addr *r_debug)
    {
      GElf_Xword soname = 0;
      GElf_Addr strtab = 0;
      GElf_Xword strsz = 0;

      Elf_Data *data = gelf_getdata_rawchunk (core, offset, filesz, ELF_T_DYN);
      if (data == NULL)
	return 0;
      size_t n = data->d_size / gelf_fsize (core, ELF_T_DYN, 1, EV_CURRENT);
      for (size_t i = 0;
	   i < n && (soname == 0 || strtab == 0 || strsz == 0);
	   ++i)
	{
	  GElf_Dyn dyn_mem;
	  GElf_Dyn *dyn = gelf_getdyn (data, i, &dyn_mem);
	  if (dyn != NULL)
	    switch (dyn->d_tag)
	      {
	      case DT_STRTAB:
		strtab = dyn->d_un.d_ptr;
		continue;

	      case DT_STRSZ:
		strsz = dyn->d_un.d_val;
		continue;

	      case DT_SONAME:
		soname = dyn->d_un.d_val;
		continue;

	      case DT_DEBUG:
		if (*r_debug == 0)
		  *r_debug = dyn->d_un.d_ptr;
		continue;

	      default:
		continue;

	      case DT_NULL:
		break;
	      }
	  break;
	}

      if (strtab != 0)
	{
	  *strtab_end = loadbase + strtab + strsz;
	  if (soname != 0)
	    return loadbase + strtab + soname;
	}
      return 0;
    }

  /* We think this PT_LOAD segment starts with an ELF header.  Investigate.  */
  inline GElf_Half consider_segment (GElf_Phdr *phdr, char *header,
				     GElf_Addr *vaddr_end,
				     GElf_Off *file_size_available,
				     GElf_Off *file_size_total,
				     GElf_Addr *loadbase,
				     GElf_Addr *dyn_vaddr,
				     GElf_Word *dyn_filesz)
    {
      union
      {
	Elf32_Ehdr e32;
	Elf64_Ehdr e64;
      } ehdr;

      Elf_Data xlatefrom =
	{
	  .d_type = ELF_T_EHDR,
	  .d_buf = header,
	  .d_version = EV_CURRENT,
	};
      Elf_Data xlateto =
	{
	  .d_type = ELF_T_EHDR,
	  .d_buf = &ehdr,
	  .d_size = sizeof ehdr,
	  .d_version = EV_CURRENT,
	};
      GElf_Half e_type = ET_NONE;
      GElf_Off phoff = 0;
      GElf_Off shdrs_end = 0;
      GElf_Half phnum = 0;
      GElf_Half phentsize = 0;
      switch (header[EI_CLASS])
	{
	case ELFCLASS32:
	  xlatefrom.d_size = sizeof (Elf32_Ehdr);
	  if (elf32_xlatetom (&xlateto, &xlatefrom, header[EI_DATA]) != NULL)
	    {
	      e_type = ehdr.e32.e_type;
	      phoff = ehdr.e32.e_phoff;
	      phnum = ehdr.e32.e_phnum;
	      phentsize = ehdr.e32.e_phentsize;
	      shdrs_end = ehdr.e32.e_shoff;
	      shdrs_end += ehdr.e32.e_shnum * ehdr.e32.e_shentsize;
	    }
	  break;

	case ELFCLASS64:
	  xlatefrom.d_size = sizeof (Elf64_Ehdr);
	  if (elf64_xlatetom (&xlateto, &xlatefrom, header[EI_DATA]) != NULL)
	    {
	      e_type = ehdr.e64.e_type;
	      phoff = ehdr.e64.e_phoff;
	      phnum = ehdr.e64.e_phnum;
	      phentsize = ehdr.e64.e_phentsize;
	      shdrs_end = ehdr.e64.e_shoff;
	      shdrs_end += ehdr.e64.e_shnum * ehdr.e64.e_shentsize;
	    }
	  break;
	}

      /* We're done with the original header we read in.  */
      gelf_freechunk (core, header);

      /* We see if we actually have phdrs to look at.  */
      if ((e_type != ET_EXEC && e_type != ET_DYN)
	  || phnum == 0 || phoff < ehdr_size
	  || phdr->p_filesz < phoff + phnum * phentsize
	  || phentsize != gelf_fsize (core, ELF_T_PHDR, 1, EV_CURRENT))
	return ET_NONE;

      /* Fetch the raw program headers to translate and examine.  */
      char *rawphdrs = gelf_rawchunk (core, phdr->p_offset + phoff,
				      phnum * phentsize);
      if (rawphdrs == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  result = -1;
	  return ET_NONE;
	}
      xlatefrom.d_buf = rawphdrs;
      xlatefrom.d_size = phnum * phentsize;
      xlatefrom.d_type = ELF_T_PHDR;
      union
      {
	Elf32_Phdr p32[phnum];
	Elf64_Phdr p64[phnum];
      } phdrs;
      xlateto.d_buf = &phdrs;
      xlateto.d_size = sizeof phdrs;
      bool phdrs_ok = false;
      switch (ehdr.e32.e_ident[EI_CLASS])
	{
	case ELFCLASS32:
	  phdrs_ok = elf32_xlatetom (&xlateto, &xlatefrom,
				     ehdr.e32.e_ident[EI_DATA]) != NULL;
	  break;

	case ELFCLASS64:
	  phdrs_ok = elf64_xlatetom (&xlateto, &xlatefrom,
				     ehdr.e64.e_ident[EI_DATA]) != NULL;
	  break;
	}
      gelf_freechunk (core, rawphdrs);

      if (!phdrs_ok)
	return ET_NONE;

      /* The p_align of a core file PT_LOAD segment gives the ELF page size
	 of the process that dumped the core.  This is what controlled the
	 interpretation of p_offset and p_vaddr values in PT_LOAD headers
	 of objects it loaded.  */
      const GElf_Xword pagesz = phdr->p_align;

      /* Consider each phdr of the embedded image.  */
      *loadbase = phdr->p_vaddr;
      bool found_base = false;
      GElf_Addr vaddr_limit = 0;
      GElf_Off file_should_end = 0;
      GElf_Off file_end = 0;
      GElf_Off file_end_aligned = 0;
      inline void handle_segment (GElf_Word type,
				  GElf_Addr vaddr, GElf_Off offset,
				  GElf_Xword filesz, GElf_Xword memsz)
	{
	  switch (type)
	    {
	    case PT_LOAD:
	      /* For load segments, keep track of the bounds of the image.  */
	      file_should_end = offset + filesz;
	      if (!found_base && (offset & -pagesz) == 0)
		{
		  *loadbase = phdr->p_vaddr - (vaddr & -pagesz);
		  found_base = true;
		}
	      vaddr_limit = (*loadbase + vaddr + memsz + pagesz - 1) & -pagesz;

	      /* If this segment starts contiguous with the previous one,
		 it extends the verbatim file image we have to use.  */
	      if (file_end_aligned == 0
		  || (offset & -pagesz) <= file_end_aligned)
		{
		  file_end = offset + filesz;
		  file_end_aligned = (offset + filesz + pagesz - 1) & -pagesz;
		}
	      break;

	    case PT_NOTE:
	      /* For note segments, inspect the contents if they are within
		 this segment of the core file.  */
	      if (offset < phdr->p_filesz && phdr->p_filesz - offset >= filesz)
		handle_note (vaddr - phdr->p_vaddr + phdr->p_offset, filesz,
			     vaddr);
	      break;

	    case PT_DYNAMIC:
	      /* Save the address of the dynamic section.  */
	      *dyn_vaddr = *loadbase + vaddr;
	      *dyn_filesz = filesz;
	      break;
	    }
	}

      switch (ehdr.e32.e_ident[EI_CLASS])
	{
	case ELFCLASS32:
	  for (uint_fast16_t i = 0; i < phnum && result >= 0; ++i)
	    handle_segment (phdrs.p32[i].p_type,
			    phdrs.p32[i].p_vaddr, phdrs.p32[i].p_offset,
			    phdrs.p32[i].p_filesz, phdrs.p32[i].p_memsz);
	  break;

	case ELFCLASS64:
	  for (uint_fast16_t i = 0; i < phnum && result >= 0; ++i)
	    handle_segment (phdrs.p64[i].p_type,
			    phdrs.p64[i].p_vaddr, phdrs.p64[i].p_offset,
			    phdrs.p64[i].p_filesz, phdrs.p64[i].p_memsz);
	  break;

	default:
	  abort ();
	  break;
	}

      /* Trim the last segment so we don't bother with zeros in the last page
	 that are off the end of the file.  However, if the extra bit in that
	 page includes the section headers, keep them.  */
      if (file_end < shdrs_end && shdrs_end <= file_end_aligned)
	file_end = shdrs_end;

      /* If there were section headers in the file, we'd like to have them.  */
      if (shdrs_end != 0 && shdrs_end <= file_end
	  && shdrs_end > file_should_end)
	file_should_end = shdrs_end;

      *file_size_available = file_end;
      *file_size_total = file_should_end;
      *vaddr_end = vaddr_limit;
      return e_type;
    }

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (core, &ehdr_mem);
  if (ehdr == NULL)
    {
    elf_error:
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return -1;
    }

  size_t earlier_modules = dwfl->nmodules;
  GElf_Addr r_debug_vaddr = 0;
  for (uint_fast16_t i = 0; i < ehdr->e_phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (core, i, &phdr_mem);
      if (phdr == NULL)
	goto elf_error;

      /* Consider read-only segments where we have enough to look at.  */
      if (phdr->p_type == PT_LOAD
	  && (phdr->p_flags & (PF_R|PF_W)) == PF_R
	  && phdr->p_filesz > ehdr_size)
	{
	  /* Look at the ELF ident bytes to see if this might be an ELF file
	     image in the format of the core file.  */
	  char *header = gelf_rawchunk (core, phdr->p_offset, ehdr_size);
	  if (header == NULL)
	    goto elf_error;
	  if (memcmp (header, ehdr->e_ident, EI_VERSION))
	    {
	      /* Doesn't look like the right header.  */
	      gelf_freechunk (core, header);
	      continue;
	    }

	  /* Consider this segment.  Bail if we get an unexpected error.  */
	  build_id_offset = 0;
	  build_id_size = 0;
	  build_id_vaddr = 0;
	  GElf_Addr dyn_vaddr = 0;
	  GElf_Word dyn_filesz = 0;
	  GElf_Addr vaddr_end;
	  GElf_Off available_size;
	  GElf_Off whole_size;
	  GElf_Addr loadbase;
	  GElf_Half type = consider_segment (phdr, header,
					     &vaddr_end,
					     &available_size, &whole_size,
					     &loadbase,
					     &dyn_vaddr, &dyn_filesz);
	  if (result < 0)
	    break;

	  if (type == ET_NONE)	/* Nothing there.  */
	    continue;

	  /* Check if this segment contains the dynamic section.  */
	  GElf_Addr soname_vaddr = 0;
	  GElf_Addr dynstr_end = 0;
	  inline void check_dyn (void)
	    {
	      if (soname_vaddr == 0 && dyn_vaddr != 0
		  && dyn_vaddr >= phdr->p_vaddr
		  && dyn_vaddr - phdr->p_vaddr + dyn_filesz <= phdr->p_filesz)
		soname_vaddr = handle_dyn (dyn_vaddr - phdr->p_vaddr
					   + phdr->p_offset,
					   dyn_filesz, loadbase, &dynstr_end,
					   &r_debug_vaddr);
	    }

	  check_dyn ();

	  /* Skip some following segments if the object we found
	     has phdrs that say they are part of its segments.  */
	  const uint_fast16_t considering = i;
	  GElf_Addr vaddr_start = phdr->p_vaddr & -phdr->p_align;
	  GElf_Off file_start = phdr->p_offset & -phdr->p_align;
	  GElf_Off file_end = (phdr->p_offset + phdr->p_filesz
			       + phdr->p_align - 1) & -phdr->p_align;
	  while ((phdr->p_type != PT_LOAD
		  || vaddr_end > phdr->p_vaddr + phdr->p_memsz)
		 && ++i < ehdr->e_phnum)
	    {
	      phdr = gelf_getphdr (core, i, &phdr_mem);
	      if (phdr == NULL)
		goto elf_error;

	      if (phdr->p_type != PT_LOAD)
		continue;

	      check_dyn ();

	      /* If we had all of the previous segment, we have this
		 segment as part of the contiguous file image.  */
	      GElf_Off segment_start = phdr->p_offset & -phdr->p_align;
	      GElf_Off segment_end = (phdr->p_offset + phdr->p_filesz
				      + phdr->p_align - 1) & -phdr->p_align;
	      if (file_end == segment_start)
		file_end = segment_end;
	    }
	  if (i == ehdr->e_phnum)
	    {
	      /* Somehing is amiss.  Punt this supposed object we found.  */
	      i = considering;
	      continue;
	    }

	  /* We have as much of the file as the dumped segments contain.  */
	  available_size = MIN (file_end - file_start, available_size);

	  /* We found an object that goes from VADDR_START to VADDR_END.  */
	  result = 1;

	  /* A dumped partial ELF file is only useful to us if it
	     contained a dynamic segment and a string table.  */
	  if (available_size < whole_size
	      && (dynstr_end == 0
		  || available_size < dynstr_end - vaddr_start + file_start))
	    available_size = 0;

	  char *soname = NULL;
	  if (dynstr_end == 0)
	    dynstr_end = vaddr_end;
	  if (soname_vaddr >= vaddr_start && soname_vaddr < dynstr_end)
	    {
	      GElf_Off soname_offset = soname_vaddr - vaddr_start + file_start;
	      GElf_Off limit = dynstr_end - vaddr_start + file_start;
	      if (limit > file_end)
		limit = file_end;
	      soname = string_from_offset (core, soname_offset, limit);
	    }

	  // XXX maybe record or verify build_id against explicit exe?
	  if (module_overlapping (dwfl, vaddr_start, vaddr_end, false) == NULL)
	    {
	      /* Record what we've learned, for find_elf to use.  */
	      struct core_module_info *mod_data = malloc (sizeof *mod_data);
	      if (mod_data == NULL)
		result = -1;
	      else
		{
		  mod_data->offset = file_start;
		  mod_data->whole_size = available_size;
		  mod_data->l_name_vaddr = 0;

		  Dwfl_Module *mod = INTUSE(dwfl_report_module)
		    (dwfl,
		     soname ?: type == ET_EXEC ? "[exe]"
		     : available_size == 0 ? "[dso]" : "[dumped-dso]",
		     vaddr_start, vaddr_end);

		  if (mod == NULL)
		    {
		      free (mod_data);
		      result = -1;
		    }
		  else
		    {
		      /* We already eliminated duplicates.  */
		      assert (mod->cb_data == NULL);
		      mod->cb_data = mod_data;
		    }

		  if (build_id_size != 0)
		    {
		      void *build_id = gelf_rawchunk (core, build_id_offset,
						      build_id_size);
		      if (build_id != NULL)
			INTUSE(dwfl_module_report_build_id) (mod, build_id,
							     build_id_size,
							     build_id_vaddr);
		      gelf_freechunk (core, build_id);
		    }
		}
	    }

	  if (soname != NULL)
	    gelf_freechunk (core, soname);

	  if (result < 0)
	    break;
	}
    }

  if (result >= 0 && r_debug_vaddr == 0 && earlier_modules > 0)
    /* Try to find an existing executable module with a DT_DEBUG.  */
    for (Dwfl_Module *mod = dwfl->modulelist;
	 earlier_modules-- > 0 && r_debug_vaddr == 0;
	 mod = mod->next)
      if (mod->main.elf != NULL)
	{
	  GElf_Addr dt_debug = find_dt_debug (mod->main.elf, mod->main.bias);
	  if (dt_debug != 0)
	    r_debug_vaddr = addr_from_memory (core, dt_debug);
	}

  if (result >= 0 && r_debug_vaddr != 0)
    /* Now we can try to find the dynamic linker's library list.  */
    result = report_r_debug (result, core, dwfl, r_debug_vaddr);

  if (result >= 0)
    dwfl->cb_data = core;

  return result;
}
INTDEF (dwfl_core_file_report)


/* Dwfl_Callbacks.find_elf */

int
dwfl_core_file_find_elf (Dwfl_Module *mod,
			 void **userdata __attribute__ ((unused)),
			 const char *module_name __attribute__ ((unused)),
			 Dwarf_Addr base __attribute__ ((unused)),
			 char **file_name, Elf **elfp)
{
  Elf *core = mod->dwfl->cb_data;
  struct core_module_info *info = mod->cb_data;

  int fd = -1;
  *file_name = NULL;

  /* If we have the whole image in the core file, just use it directly.  */
  if (info != NULL && info->whole_size != 0)
    *elfp = gelf_begin_embedded (ELF_C_READ_MMAP_PRIVATE, core,
				 info->offset, info->whole_size);

  /* If we found a build ID, try to follow that.  */
  if (*elfp == NULL && mod->build_id_len > 0)
    {
      fd = INTUSE(dwfl_build_id_find_elf) (mod, NULL, NULL, 0,
					   file_name, elfp);
      if (fd >= 0)
	return fd;
    }

  /* If we found the dynamic linker's idea of the file name, report that.  */
  if (info != NULL && info->l_name_vaddr != 0)
    {
      char *chunk = string_from_memory (core, info->l_name_vaddr);
      if (chunk != NULL)
	{
	  *file_name = strdup (chunk);
	  gelf_freechunk (core, chunk);
	}
    }

  return fd;
}
INTDEF (dwfl_core_file_find_elf)
