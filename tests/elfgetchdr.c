/* Copyright (C) 2015 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  elf_version (EV_CURRENT);

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);

      Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
      if (elf == NULL)
	{
	  printf ("%s not usable %s\n", argv[cnt], elf_errmsg (-1));
	  result = 1;
	  close (fd);
	  continue;
	}

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  int idx = elf_ndxscn (scn);;
	  GElf_Chdr chdr;
	  int chdr_type;
	  if (gelf_getchdr (scn, &chdr, &chdr_type) != NULL)
	    {
	      assert (chdr_type != ELF_ZSCN_T_NONE);
	      printf ("section %d: %s Compressed ch_type: %" PRId32
		      ", ch_size: %" PRIx64 ", ch_addralign: %" PRIx64 "\n",
		      idx, chdr_type == ELF_ZSCN_T_ELF ? "ELF" : "GNU",
		      chdr.ch_type, chdr.ch_size, chdr.ch_addralign);
	    }
	  else if (chdr_type != -1)
	    {
	      assert (chdr_type == ELF_ZSCN_T_NONE);
	      printf ("section %d: NOT Compressed %s\n", idx, elf_errmsg (-1));
	    }
	  else
	    printf ("section %d: gelf_getchdr error %s\n", idx,
		    elf_errmsg (-1));
	}

      elf_end (elf);
      close (fd);
    }

  return result;
}
