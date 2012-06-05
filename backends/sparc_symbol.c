/* SPARC specific symbolic name handling.
   Copyright (C) 2002-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Jakub Jelinek <jakub@redhat.com>, 2002.

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

#include <elf.h>
#include <stddef.h>

#define BACKEND		sparc_
#include "libebl_CPU.h"

/* Check for the simple reloc types.  */
int
sparc_reloc_simple_types (Ebl *ebl __attribute__ ((unused)),
			  const int **rel8_types, const int **rel4_types)
{
  static const int rel8[] = { R_SPARC_64, R_SPARC_UA64, 0 };
  static const int rel4[] = { R_SPARC_32, R_SPARC_UA32, 0 };
  *rel8_types = rel8;
  *rel4_types = rel4;
  return 0;
}

/* Check whether machine flags are valid.  */
bool
sparc_machine_flag_check (GElf_Word flags)
{
  return ((flags &~ (EF_SPARCV9_MM
		     | EF_SPARC_LEDATA
		     | EF_SPARC_32PLUS
		     | EF_SPARC_SUN_US1
		     | EF_SPARC_SUN_US3)) == 0);
}

bool
sparc_check_special_section (Ebl *ebl,
			     int ndx __attribute__ ((unused)),
			     const GElf_Shdr *shdr,
			     const char *sname __attribute__ ((unused)))
{
  if ((shdr->sh_flags & (SHF_WRITE | SHF_EXECINSTR))
      == (SHF_WRITE | SHF_EXECINSTR))
    {
      /* This is ordinarily flagged, but is valid for a PLT on SPARC.

	 Look for the SHT_DYNAMIC section and the DT_PLTGOT tag in it.
	 Its d_ptr should match the .plt section's sh_addr.  */

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
	{
	  GElf_Shdr scn_shdr;
	  if (likely (gelf_getshdr (scn, &scn_shdr) != NULL)
	      && scn_shdr.sh_type == SHT_DYNAMIC
	      && scn_shdr.sh_entsize != 0)
	    {
	      Elf_Data *data = elf_getdata (scn, NULL);
	      if (data != NULL)
		for (size_t i = 0; i < data->d_size / scn_shdr.sh_entsize; ++i)
		  {
		    GElf_Dyn dyn;
		    if (unlikely (gelf_getdyn (data, i, &dyn) == NULL))
		      break;
		    if (dyn.d_tag == DT_PLTGOT)
		      return dyn.d_un.d_ptr == shdr->sh_addr;
		  }
	      break;
	    }
	}
    }

  return false;
}

const char *
sparc_symbol_type_name (int type,
			char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (type)
    {
    case STT_SPARC_REGISTER:
      return "SPARC_REGISTER";
    }
  return NULL;
}

const char *
sparc_dynamic_tag_name (int64_t tag,
			char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_SPARC_REGISTER:
      return "SPARC_REGISTER";
    }
  return NULL;
}

bool
sparc_dynamic_tag_check (int64_t tag)
{
  switch (tag)
    {
    case DT_SPARC_REGISTER:
      return true;
    }
  return false;
}
