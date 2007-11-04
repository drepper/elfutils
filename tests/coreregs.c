/* Test program for libdwfl core file handling.
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

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <fcntl.h>
#include <gelf.h>
#include <dwarf.h>
#include ELFUTILS_HEADER(dwfl)
#include "../libdwfl/libdwflP.h" /* XXX */


typedef uint8_t GElf_Byte;

static void
convert (Elf *core, Elf_Type type, void *value, void *data)
{
  Elf_Data valuedata =
    {
      .d_type = type,
      .d_buf = value,
      .d_size = gelf_fsize (core, type, 1, EV_CURRENT),
      .d_version = EV_CURRENT,
    };
  Elf_Data indata =
    {
      .d_type = type,
      .d_buf = data,
      .d_size = valuedata.d_size,
      .d_version = EV_CURRENT,
    };

  Elf_Data *d = (gelf_getclass (core) == ELFCLASS32
		 ? elf32_xlatetom : elf64_xlatetom)
    (&valuedata, &indata, elf_getident (core, NULL)[EI_DATA]);
  if (d == NULL)
    error (2, 0, "elf_xlatetom: %s", elf_errmsg (-1));
}

static void
handle_thread_identifier (Elf *core, Elf_Type type, void *data)
{
#define TYPES								      \
  DO_TYPE (BYTE, Byte, "0x%.2" PRIx8); DO_TYPE (HALF, Half, "0x%.4" PRIx16);  \
  DO_TYPE (WORD, Word, "0x%.8" PRIx32); DO_TYPE (SWORD, Sword, "%" PRId32);   \
  DO_TYPE (XWORD, Xword, "0x%.16" PRIx64); DO_TYPE (SXWORD, Sxword, "%" PRId64)

#define DO_TYPE(NAME, Name, fmt) GElf_##Name Name
  union { TYPES; } value;
#undef DO_TYPE

  convert (core, type, &value, data);

  printf ("    thread identifier: ");
  switch (type)
    {
    default:
      abort ();
      break;

#define DO_TYPE(NAME, Name, fmt)					      \
    case ELF_T_##NAME:							      \
      printf (fmt, value.Name);						      \
      break
    TYPES;
#undef DO_TYPE
    }
  putchar_unlocked ('\n');

#undef TYPES
}

static void
handle_register_data (Dwfl *dwfl, Elf *core,
		      int regno, const char *regname, int bits, int type,
		      void *data)
{
#define TYPES						\
  BITS (8, BYTE, "%" PRId8, "0x%.2" PRIx8);		\
  BITS (16, HALF, "%" PRId16, "0x%.4" PRIx16);		\
  BITS (32, WORD, "%" PRId32, "0x%.8" PRIx32);		\
  BITS (64, XWORD, "%" PRId64, "0x%.16" PRIx64)

#define BITS(bits, xtype, sfmt, ufmt) uint##bits##_t b##bits
  union { TYPES; } value;
#undef	BITS

  printf ("%9s (%2d): ", regname, regno);

  Dwarf_Addr addr = 0;
  switch (type)
    {
    case DW_ATE_unsigned:
    case DW_ATE_signed:
    case DW_ATE_address:
      switch (bits)
	{
#define BITS(bits, xtype, sfmt, ufmt)				\
	case bits:						\
	    convert (core, ELF_T_##xtype, &value, data);	\
	  if (type == DW_ATE_signed)				\
	    printf (sfmt, value.b##bits);			\
	  else							\
	    printf (ufmt, value.b##bits);			\
	  addr = value.b##bits;					\
	  break

	TYPES;

	default:
	  abort ();
#undef	BITS
	}
      break;

    default:
      assert (bits % 8 == 0);
      break;
    }

  if (type == DW_ATE_address)
    {
      GElf_Sym sym;
      const char *name = dwfl_module_addrsym (dwfl_addrmodule (dwfl, addr),
					      addr, &sym, NULL);
      if (name != NULL)
	{
	  if (addr == sym.st_value)
	    printf ("\t<%s>", name);
	  else
	    printf ("\t<%s%+" PRId64 ">", name, addr - sym.st_value);
	}
    }

  putchar_unlocked ('\n');

#undef	TYPES
}

static void
handle_thread (Dwfl *dwfl, Elf *core, Dwfl_Register_Map *map,
	       int nsets, GElf_Off offsets[], GElf_Word sizes[],
	       int idset, GElf_Word idpos, Elf_Type idtype)
{
  void *sets[nsets];
  memset (sets, 0, sizeof sets);
  inline int establish (int setno)
    {
      if (sets[setno] == NULL)
	{
	  if (sizes[setno] == 0)
	    return 1;
	  sets[setno] = gelf_rawchunk (core, offsets[setno], sizes[setno]);
	  if (sets[setno] == NULL)
	    return -1;
	}
      return 0;
    }
  int handle_register (void *arg __attribute__ ((unused)),
		       int regno,
		       const char *setname __attribute__ ((unused)),
		       const char *prefix __attribute__ ((unused)),
		       const char *regname,
		       int bits, int type)
    {
      GElf_Word offset;
      int setno = dwfl_register_map (map, regno, &offset);
      if (setno >= 0)
	{
	  int result = establish (setno);
	  if (result == 0)
	    handle_register_data (dwfl, core, regno, regname, bits, type,
				  sets[setno] + offset);
	  else if (result < 0)
	    error (2, 0, "gelf_rawchunk: %s", elf_errmsg (-1));
	}
      return 0;
    }

  if (idset < 0)
    puts ("  no thread identifier!");
  else
    {
      int result = establish (idset);
      if (result < 0)
	error (2, 0, "gelf_rawchunk: %s", elf_errmsg (-1));
      assert (result == 0);

      handle_thread_identifier (core, idtype, sets[idset] + idpos);
    }

  Dwfl_Module *mod = dwfl->modulelist; /* XXX */
  GElf_Addr base;
  while (dwfl_module_getelf (mod, &base) == NULL)
    mod = mod->next;
  int result = dwfl_module_register_names (mod, &handle_register, NULL);
  assert (result == 0);

  for (int i = 0; i < nsets; ++i)
    if (sets[i] != NULL)
      gelf_freechunk (core, sets[i]);
}

static void
find_registers (Dwfl *dwfl, Elf *core)
{
  GElf_Off offset;
  GElf_Off limit;
  Dwfl_Register_Map *map;
  int nsets = dwfl_core_file_register_map (dwfl, &map, &offset, &limit);
  if (nsets < 0)
    error (2, 0, "dwfl_core_file_register_map: %s", dwfl_errmsg (-1));

  if (nsets == 0)
    {
      puts ("  no register information");
      return;
    }

  GElf_Off offsets[nsets];
  GElf_Word sizes[nsets];
  int result;
  do
    {
      int idset;
      GElf_Word idpos;
      Elf_Type idtype;
      GElf_Nhdr nhdr;
      GElf_Off note_offset;
      result = dwfl_core_file_read_note (dwfl, map, offset, limit, nsets,
					 offsets, sizes,
					 &idset, &idpos, &idtype,
					 &offset, &nhdr, &note_offset);
      if (result >= 0)
	handle_thread (dwfl, core, map, nsets,
		       offsets, sizes, idset, idpos, idtype);
      if (result == 0)
	{
	  /* Non-thread note.  */
	  offset = note_offset + nhdr.n_descsz;
	  break;
	}
    } while (result >= 0 && offset < limit);

  if (result < 0)
    error (2, 0, "dwfl_core_file_read_note: %s", dwfl_errmsg (-1));

  dwfl_register_map_end (map);
}

static const Dwfl_Callbacks corefile_callbacks =
  {
    .find_debuginfo = INTUSE(dwfl_standard_find_debuginfo),
    .find_elf = INTUSE(dwfl_core_file_find_elf),
  };

static void
handle_core_file (const char *name)
{
  int fd = open64 (name, O_RDONLY);
  if (fd < 0)
    error (2, errno, "cannot open '%s'", name);

  elf_version (EV_CURRENT);
  Elf *core = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
  if (core == NULL)
    error (2, 0, "cannot read ELF core file: %s", elf_errmsg (-1));

  Dwfl *dwfl = dwfl_begin (&corefile_callbacks);
  int result = dwfl_core_file_report (dwfl, core);
  if (result < 0)
    error (2, 0, "%s: %s", name, dwfl_errmsg (-1));
  dwfl_report_end (dwfl, NULL, NULL);

  printf ("%s:\n", name);
  find_registers (dwfl, core);

  dwfl_end (dwfl);
  elf_end (core);
  close (fd);
}

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  for (int i = 1; i < argc; ++i)
    handle_core_file (argv[i]);

  return 0;
}
