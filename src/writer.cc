/* Compare semantic content of two DWARF files.
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

#include <sys/time.h>
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <locale.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>
#include <iostream>
#include <set>

#include <elf-knowledge.h>

#include "../libebl/libebl.h"
extern "C" {
#include "../lib/system.h"
}

/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;

/* Bug report address.  */
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

/* Values for the parameters which have no short form.  */
#define OPT_XXX			0x100

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Control options:"), 0 },
  { "quiet", 'q', NULL, 0, N_("Output nothing; yield exit status only"), 0 },
  { "ignore-missing", 'i', NULL, 0,
    N_("Don't complain if both files have no DWARF at all"), 0 },

  { "test-writer", 'T', NULL, 0, N_("Test DWARF output classes"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Compare two DWARF files for semantic equality.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE1 FILE2");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

/* Nonzero if only exit status is wanted.  */
static bool quiet;

/* Nonzero if missing DWARF is equal DWARF.  */
static bool missing_ok;

/* Nonzero to test writer classes.  */
static int test_writer;


/*
static Dwarf *
open_file (const char *fname, int *fdp, Elf **elfp)
{
  int fd = open (fname, O_RDWR);
  if (unlikely (fd == -1))
    error (2, errno, gettext ("cannot open '%s'"), fname);
  *fdp = fd;

  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf == NULL)
    error (2, errno, gettext ("cannot create Elf handle: %s"), elf_errmsg (-1));
  *elfp = elf;

  Dwarf *dw = dwarf_begin_elf (elf, DWARF_C_READ, NULL);
  if (dw == NULL)
    error (2, errno, gettext ("cannot create Dwarf handle: %s"), dwarf_errmsg (-1));

  return dw;
}
*/

#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR %d (%s-%s): %s"),      \
	 fname, __LINE__, PACKAGE_VERSION, __DATE__, elf_errmsg (-1))

#define MAX_STACK_ALLOC	(400 * 1024)

bool remove_debug = true;
bool permissive = true;
char const *output_fname = "out";

/* Update section headers when the data size has changed.
   We also update the SHT_NOBITS section in the debug
   file so that the section headers match in sh_size.  */
inline void update_section_size (const Elf_Data *newdata,
				 Elf_Scn *&scn, Elf *&debugelf, size_t &cnt)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  shdr->sh_size = newdata->d_size;
  (void) gelf_update_shdr (scn, shdr);
  if (debugelf != NULL)
    {
      /* libelf will use d_size to set sh_size.  */
      Elf_Data *debugdata = elf_getdata (elf_getscn (debugelf,
						     cnt), NULL);
      debugdata->d_size = newdata->d_size;
    }
}

struct shdr_info_t
{
  Elf_Scn *scn;
  GElf_Shdr shdr;
  Elf_Data *data;
  const char *name;
  Elf32_Word idx;		/* Index in new file.  */
  Elf32_Word old_sh_link;	/* Original value of shdr.sh_link.  */
  Elf32_Word symtab_idx;
  Elf32_Word version_idx;
  Elf32_Word group_idx;
  Elf32_Word group_cnt;
  Elf_Scn *newscn;
  struct Ebl_Strent *se;
  Elf32_Word *newsymidx;

  shdr_info_t () {
    memset (this, 0, sizeof (*this));
  }

  shdr_info_t (shdr_info_t const &copy) {
    memcpy (this, &copy, sizeof (*this));
  }
};

inline bool no_symtab_updates (shdr_info_t const &s)
{
  /* If the symbol table hasn't changed, do not do anything.  */
  return s.newsymidx == NULL;
}

static int
handle_elf (int fd, Elf *elf, const char *fname,
	    mode_t mode, struct timeval tvp[2])
{
  Elf *debugelf = NULL;
  char *tmp_debug_fname = NULL;
  int result = 0;
  size_t shstrndx;
  std::vector <shdr_info_t> shdr_info;
  bool changes;
  GElf_Ehdr newehdr_mem;
  GElf_Ehdr *newehdr;
  struct Ebl_Strtab *shst = NULL;
  bool any_symtab_changes = false;
  Elf *newelf;

  /* If we are not replacing the input file open a new file here.  */
  if (output_fname != NULL)
    {
      fd = open (output_fname, O_RDWR | O_CREAT, mode);
      if (unlikely (fd == -1))
	{
	  error (0, errno, gettext ("cannot open '%s'"), output_fname);
	  return 1;
	}
    }

  int debug_fd = -1;

  /* Get the EBL handling.  The -g option is currently the only reason
     we need EBL so don't open the backend unless necessary.  */
  Ebl *ebl = NULL;
  if (remove_debug)
    {
      ebl = ebl_openbackend (elf);
      if (ebl == NULL)
	{
	  error (0, errno, gettext ("cannot open EBL backend"));
	  result = 1;
	  goto fail;
	}
    }


  /* Exit handlers.  Need to be at the beginning so that the compiler
     doesn't complain that jumps skip initializations.  */
  if (false)
    {
    fail_close:
      /* For some sections we might have created an table to map symbol
	 table indices.  */
      if (any_symtab_changes)
	for (std::vector<shdr_info_t>::iterator it = shdr_info.begin ();
	     it != shdr_info.end (); ++it)
	  free (it->newsymidx);

      /* Free other resources.  */
      if (shst != NULL)
	ebl_strtabfree (shst);

      /* That was it.  Close the descriptors.  */
      if (elf_end (newelf) != 0)
	{
	  error (0, 0, gettext ("error while finishing '%s': %s"), fname,
		 elf_errmsg (-1));
	  result = 1;
	}

      if (debugelf != NULL && elf_end (debugelf) != 0)
	{
	  error (0, 0, gettext ("error while finishing '%s': %s"), "ble",
		 elf_errmsg (-1));
	  result = 1;
	}

    fail:
      /* Close the EBL backend.  */
      if (ebl != NULL)
	ebl_closebackend (ebl);

      /* Close debug file descriptor, if opened */
      if (debug_fd >= 0)
	{
	  if (tmp_debug_fname != NULL)
	    unlink (tmp_debug_fname);
	  close (debug_fd);
	}

      /* If requested, preserve the timestamp.  */
      if (tvp != NULL)
	{
	  if (futimes (fd, tvp) != 0)
	    {
	      error (0, errno, gettext ("\
cannot set access and modification date of '%s'"),
		     output_fname ?: fname);
	      result = 1;
	    }
	}

      /* Close the file descriptor if we created a new file.  */
      if (output_fname != NULL)
	close (fd);

      return result;
    }

  /* Get the information from the old file.  */
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fname);

  /* Get the section header string table index.  */
  if (unlikely (elf_getshdrstrndx (elf, &shstrndx) < 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* We now create a new ELF descriptor for the same file.  We
     construct it almost exactly in the same way with some information
     dropped.  */
  if (output_fname != NULL)
    newelf = elf_begin (fd, ELF_C_WRITE_MMAP, NULL);
  else
    newelf = elf_clone (elf, ELF_C_EMPTY);

  if (unlikely (gelf_newehdr (newelf, gelf_getclass (elf)) == 0)
      || (ehdr->e_type != ET_REL
	  && unlikely (gelf_newphdr (newelf, ehdr->e_phnum) == 0)))
    {
      error (0, 0, gettext ("cannot create new file '%s': %s"),
	     output_fname, elf_errmsg (-1));
      goto fail;
    }

  /* Copy over the old program header if needed.  */
  if (ehdr->e_type != ET_REL)
    for (size_t cnt = 0; cnt < ehdr->e_phnum; ++cnt)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	if (phdr == NULL
	    || unlikely (gelf_update_phdr (newelf, cnt, phdr) == 0))
	  INTERNAL_ERROR (fname);
      }

  /* Number of sections.  */
  size_t shnum;
  if (unlikely (elf_getshdrnum (elf, &shnum) < 0))
    {
      error (0, 0, gettext ("cannot determine number of sections: %s"),
	     elf_errmsg (-1));
      goto fail_close;
    }

  /* Storage for section information.  We leave room for two more
     entries since we unconditionally create a section header string
     table.  Maybe some weird tool created an ELF file without one.
     The other one is used for the debug link section.  */
  shdr_info.push_back (shdr_info_t ());

  /* Prepare section information data structure.  */
  for (Elf_Scn *scn = NULL; (scn = elf_nextscn (elf, scn)); )
    {
      size_t cnt = shdr_info.size ();

      /* This should always be true (i.e., there should not be any
	 holes in the numbering).  */
      assert (elf_ndxscn (scn) == cnt);

      shdr_info_t cur;
      cur.scn = scn;

      /* Get the header.  */
      if (gelf_getshdr (scn, &cur.shdr) == NULL)
	INTERNAL_ERROR (fname);

      /* Get the name of the section.  */
      cur.name = elf_strptr (elf, shstrndx,
			     cur.shdr.sh_name);
      if (cur.name == NULL)
	{
	  error (0, 0, gettext ("illformed file '%s'"), fname);
	  goto fail_close;
	}

      /* Mark them as present but not yet investigated.  */
      cur.idx = 1;

      /* Remember the shdr.sh_link value.  */
      cur.old_sh_link = cur.shdr.sh_link;

      /* Sections in files other than relocatable object files which
	 are not loaded can be freely moved by us.  In relocatable
	 object files everything can be moved.  */
      if (ehdr->e_type == ET_REL
	  || (cur.shdr.sh_flags & SHF_ALLOC) == 0)
	cur.shdr.sh_offset = 0;

      /* If this is an extended section index table store an
	 appropriate reference.  */
      if (unlikely (cur.shdr.sh_type == SHT_SYMTAB_SHNDX))
	{
	  assert (shdr_info[cur.shdr.sh_link].symtab_idx == 0);
	  shdr_info[cur.shdr.sh_link].symtab_idx = cnt;
	}
      else if (unlikely (cur.shdr.sh_type == SHT_GROUP))
	{
	  /* Cross-reference the sections contained in the section
	     group.  */
	  cur.data = elf_getdata (cur.scn, NULL);
	  if (cur.data == NULL)
	    INTERNAL_ERROR (fname);

	  /* XXX Fix for unaligned access.  */
	  Elf32_Word *grpref = (Elf32_Word *) cur.data->d_buf;
	  size_t inner;
	  for (inner = 1;
	       inner < cur.data->d_size / sizeof (Elf32_Word);
	       ++inner)
	    shdr_info[grpref[inner]].group_idx = cnt;

	  if (inner == 1 || (inner == 2 && (grpref[0] & GRP_COMDAT) == 0))
	    /* If the section group contains only one element and this
	       is n COMDAT section we can drop it right away.  */
	    cur.idx = 0;
	  else
	    cur.group_cnt = inner - 1;
	}
      else if (unlikely (cur.shdr.sh_type == SHT_GNU_versym))
	{
	  assert (shdr_info[cur.shdr.sh_link].version_idx == 0);
	  shdr_info[cur.shdr.sh_link].version_idx = cnt;
	}

      /* If this section is part of a group make sure it is not
	 discarded right away.  */
      if ((cur.shdr.sh_flags & SHF_GROUP) != 0)
	{
	  assert (cur.group_idx != 0);

	  if (shdr_info[cur.group_idx].idx == 0)
	    {
	      /* The section group section will be removed.  */
	      cur.group_idx = 0;
	      cur.shdr.sh_flags &= ~SHF_GROUP;
	    }
	}

      shdr_info.push_back (cur);
    }
  //shdr_info.push_back (*new shdr_info_t); // xxx leak

  /* We drop all .debug_* sections.
     XXX .eh_frame??
     XXX .rel{,a}.debug_*?? */
  for (size_t cnt = 1; cnt < shnum; ++cnt)
    if (strncmp (shdr_info[cnt].name, ".debug_", 7) == 0)
      {
	/* For now assume this section will be removed.  */
	shdr_info[cnt].idx = 0;

	size_t idx = shdr_info[cnt].group_idx;
	while (idx != 0)
	  {
	    /* The section group data is already loaded.  */
	    assert (shdr_info[idx].data != NULL);

	    /* If the references section group is a normal section
	       group and has one element remaining, or if it is an
	       empty COMDAT section group it is removed.  */
	    bool is_comdat = (((Elf32_Word *) shdr_info[idx].data->d_buf)[0]
			      & GRP_COMDAT) != 0;

	    --shdr_info[idx].group_cnt;
	    if ((!is_comdat && shdr_info[idx].group_cnt == 1)
		|| (is_comdat && shdr_info[idx].group_cnt == 0))
	      {
		shdr_info[idx].idx = 0;
		/* Continue recursively.  */
		idx = shdr_info[idx].group_idx;
	      }
	    else
	      break;
	  }
      }

  /* Mark the SHT_NULL section as handled.  */
  shdr_info[0].idx = 2;


  /* Handle exceptions: section groups and cross-references.  We might
     have to repeat this a few times since the resetting of the flag
     might propagate.  */
  do
    {
      changes = false;

      for (size_t cnt = 1; cnt < shnum; ++cnt)
	{
	  if (shdr_info[cnt].idx == 0)
	    {
	      /* If a relocation section is marked as being removed make
		 sure the section it is relocating is removed, too.  */
	      if ((shdr_info[cnt].shdr.sh_type == SHT_REL
		   || shdr_info[cnt].shdr.sh_type == SHT_RELA)
		  && shdr_info[shdr_info[cnt].shdr.sh_info].idx != 0)
		shdr_info[cnt].idx = 1;
	    }

	  if (shdr_info[cnt].idx == 1)
	    {
	      /* Cross referencing happens:
		 - for the cases the ELF specification says.  That are
		   + SHT_DYNAMIC in sh_link to string table
		   + SHT_HASH in sh_link to symbol table
		   + SHT_REL and SHT_RELA in sh_link to symbol table
		   + SHT_SYMTAB and SHT_DYNSYM in sh_link to string table
		   + SHT_GROUP in sh_link to symbol table
		   + SHT_SYMTAB_SHNDX in sh_link to symbol table
		   Other (OS or architecture-specific) sections might as
		   well use this field so we process it unconditionally.
		 - references inside section groups
		 - specially marked references in sh_info if the SHF_INFO_LINK
		 flag is set
	      */

	      if (shdr_info[shdr_info[cnt].shdr.sh_link].idx == 0)
		{
		  shdr_info[shdr_info[cnt].shdr.sh_link].idx = 1;
		  changes |= shdr_info[cnt].shdr.sh_link < cnt;
		}

	      /* Handle references through sh_info.  */
	      if (SH_INFO_LINK_P (&shdr_info[cnt].shdr)
		  && shdr_info[shdr_info[cnt].shdr.sh_info].idx == 0)
		{
		  shdr_info[shdr_info[cnt].shdr.sh_info].idx = 1;
		  changes |= shdr_info[cnt].shdr.sh_info < cnt;
		}

	      /* Mark the section as investigated.  */
	      shdr_info[cnt].idx = 2;
	    }
	}
    }
  while (changes);

  /* Mark the section header string table as unused, we will create
     a new one.  */
  shdr_info[shstrndx].idx = 0;

  /* We need a string table for the section headers.  */
  shst = ebl_strtabinit (true);
  if (shst == NULL)
    error (EXIT_FAILURE, errno, gettext ("while preparing output for '%s'"),
	   output_fname ?: fname);

  /* Assign new section numbers.  */
  shdr_info[0].idx = 0;
  size_t idx;
  for (size_t cnt = idx = 1; cnt < shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	shdr_info[cnt].idx = idx++;

	/* Create a new section.  */
	shdr_info[cnt].newscn = elf_newscn (newelf);
	if (shdr_info[cnt].newscn == NULL)
	  error (EXIT_FAILURE, 0, gettext ("while generating output file: %s"),
		 elf_errmsg (-1));

	assert (elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx);

	/* Add this name to the section header string table.  */
	shdr_info[cnt].se = ebl_strtabadd (shst, shdr_info[cnt].name, 0);
      }

  {
    shdr_info_t debug_info;

    /* Add the section header string table section name.  */
    char *name = strdup (".debug_info"); // ebl_strtabadd doesn't
					 // intern the string
    debug_info.se = ebl_strtabadd (shst, name, strlen (name) + 1);
    debug_info.idx = idx;

    /* Create the section header.  */
    debug_info.shdr.sh_type = SHT_PROGBITS;
    debug_info.shdr.sh_flags = 0;
    debug_info.shdr.sh_addr = 0;
    debug_info.shdr.sh_link = SHN_UNDEF;
    debug_info.shdr.sh_info = SHN_UNDEF;
    debug_info.shdr.sh_entsize = 0;
    debug_info.shdr.sh_offset = 0;
    debug_info.shdr.sh_addralign = 1;

    /* Create the section.  */
    debug_info.newscn = elf_newscn (newelf);
    if (debug_info.newscn == NULL)
      error (EXIT_FAILURE, 0,
	     gettext ("while create section header section: %s"),
	     elf_errmsg (-1));
    assert (elf_ndxscn (debug_info.newscn) == idx);

    /* Finalize the string table and fill in the correct indices in the
       section headers.  */
    Elf_Data *debug_info_data = elf_newdata (debug_info.newscn);
    // xxx free on failure
    if (debug_info_data == NULL)
      error (EXIT_FAILURE, 0,
	     gettext ("while create section header string table: %s"),
	     elf_errmsg (-1));

    debug_info_data->d_buf = (void*)"huhly";
    debug_info_data->d_type = ELF_T_BYTE;
    debug_info_data->d_size = strlen ((const char *)debug_info_data->d_buf) + 1;
    debug_info_data->d_align = 1;

    /* We have to set the section size.  */
    debug_info.shdr.sh_size = debug_info_data->d_size;

    shdr_info.push_back (debug_info);
    ++idx;
  }

  {
    shdr_info_t shstrtab;

    /* Add the section header string table section name.  */
    shstrtab.se = ebl_strtabadd (shst, ".shstrtab", 10);
    shstrtab.idx = idx;

    /* Create the section header.  */
    shstrtab.shdr.sh_type = SHT_STRTAB;
    shstrtab.shdr.sh_flags = 0;
    shstrtab.shdr.sh_addr = 0;
    shstrtab.shdr.sh_link = SHN_UNDEF;
    shstrtab.shdr.sh_info = SHN_UNDEF;
    shstrtab.shdr.sh_entsize = 0;
    /* We set the offset to zero here.  Before we write the ELF file the
       field must have the correct value.  This is done in the final
       loop over all section.  Then we have all the information needed.  */
    shstrtab.shdr.sh_offset = 0;
    shstrtab.shdr.sh_addralign = 1;

    /* Create the section.  */
    shstrtab.newscn = elf_newscn (newelf);
    if (shstrtab.newscn == NULL)
      error (EXIT_FAILURE, 0,
	     gettext ("while create section header section: %s"),
	     elf_errmsg (-1));
    assert (elf_ndxscn (shstrtab.newscn) == idx);

    /* Finalize the string table and fill in the correct indices in the
       section headers.  */
    Elf_Data *shstrtab_data = elf_newdata (shstrtab.newscn);
    // xxx free on failure
    if (shstrtab_data == NULL)
      error (EXIT_FAILURE, 0,
	     gettext ("while create section header string table: %s"),
	     elf_errmsg (-1));
    ebl_strtabfinalize (shst, shstrtab_data);

    /* We have to set the section size.  */
    shstrtab.shdr.sh_size = shstrtab_data->d_size;

    shdr_info.push_back (shstrtab);
  }

  /* Update the section information.  */
  GElf_Off lastoffset = 0;
  for (size_t cnt = 1; cnt < shdr_info.size (); ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	Elf_Data *newdata;

	Elf_Scn *scn = elf_getscn (newelf, shdr_info[cnt].idx);
	assert (scn != NULL);

	/* Update the name.  */
	shdr_info[cnt].shdr.sh_name = ebl_strtaboffset (shdr_info[cnt].se);

	/* Update the section header from the input file.  Some fields
	   might be section indices which now have to be adjusted.  */
	if (shdr_info[cnt].shdr.sh_link != 0)
	  shdr_info[cnt].shdr.sh_link =
	    shdr_info[shdr_info[cnt].shdr.sh_link].idx;

	if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	  {
	    assert (shdr_info[cnt].data != NULL);

	    Elf32_Word *grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	    for (size_t inner = 0;
		 inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		 ++inner)
	      grpref[inner] = shdr_info[grpref[inner]].idx;
	  }

	/* Handle the SHT_REL, SHT_RELA, and SHF_INFO_LINK flag.  */
	if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
	  shdr_info[cnt].shdr.sh_info =
	    shdr_info[shdr_info[cnt].shdr.sh_info].idx;

	/* Get the data from the old file if necessary.  We already
           created the data for the section header string table.  */
	if (cnt < shnum)
	  {
	    if (shdr_info[cnt].data == NULL)
	      {
		shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
		if (shdr_info[cnt].data == NULL)
		  INTERNAL_ERROR (fname);
	      }

	    /* Set the data.  This is done by copying from the old file.  */
	    newdata = elf_newdata (scn);
	    if (newdata == NULL)
	      INTERNAL_ERROR (fname);

	    /* Copy the structure.  */
	    *newdata = *shdr_info[cnt].data;

	    /* We know the size.  */
	    shdr_info[cnt].shdr.sh_size = shdr_info[cnt].data->d_size;

	    /* We have to adjust symbol tables.  The st_shndx member might
	       have to be updated.  */
	    if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		|| shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
	      {
		Elf_Data *versiondata = NULL;
		Elf_Data *shndxdata = NULL;

		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					    ehdr->e_version);

		if (shdr_info[cnt].symtab_idx != 0)
		  {
		    assert (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB_SHNDX);
		    /* This section has extended section information.
		       We have to modify that information, too.  */
		    shndxdata = elf_getdata (shdr_info[shdr_info[cnt].symtab_idx].scn,
					     NULL);

		    assert ((versiondata->d_size / sizeof (Elf32_Word))
			    >= shdr_info[cnt].data->d_size / elsize);
		  }

		if (shdr_info[cnt].version_idx != 0)
		  {
		    assert (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM);
		    /* This section has associated version
		       information.  We have to modify that
		       information, too.  */
		    versiondata = elf_getdata (shdr_info[shdr_info[cnt].version_idx].scn,
					       NULL);

		    assert ((versiondata->d_size / sizeof (GElf_Versym))
			    >= shdr_info[cnt].data->d_size / elsize);
		  }

		shdr_info[cnt].newsymidx
		  = (Elf32_Word *) xcalloc (shdr_info[cnt].data->d_size
					    / elsize, sizeof (Elf32_Word));

		bool last_was_local = true;
		size_t destidx;
		size_t inner;
		for (destidx = inner = 1;
		     inner < shdr_info[cnt].data->d_size / elsize;
		     ++inner)
		  {
		    Elf32_Word sec;
		    GElf_Sym sym_mem;
		    Elf32_Word xshndx;
		    GElf_Sym *sym = gelf_getsymshndx (shdr_info[cnt].data,
						      shndxdata, inner,
						      &sym_mem, &xshndx);
		    if (sym == NULL)
		      INTERNAL_ERROR (fname);

		    if (sym->st_shndx == SHN_UNDEF
			|| (sym->st_shndx >= shnum
			    && sym->st_shndx != SHN_XINDEX))
		      {
			/* This is no section index, leave it alone
			   unless it is moved.  */
			if (destidx != inner
			    && gelf_update_symshndx (shdr_info[cnt].data,
						     shndxdata,
						     destidx, sym,
						     xshndx) == 0)
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }

			continue;
		      }

		    /* Get the full section index, if necessary from the
		       XINDEX table.  */
		    if (sym->st_shndx != SHN_XINDEX)
		      sec = shdr_info[sym->st_shndx].idx;
		    else
		      {
			assert (shndxdata != NULL);

			sec = shdr_info[xshndx].idx;
		      }

		    if (sec != 0)
		      {
			GElf_Section nshndx;
			Elf32_Word nxshndx;

			if (sec < SHN_LORESERVE)
			  {
			    nshndx = sec;
			    nxshndx = 0;
			  }
			else
			  {
			    nshndx = SHN_XINDEX;
			    nxshndx = sec;
			  }

			assert (sec < SHN_LORESERVE || shndxdata != NULL);

			if ((inner != destidx || nshndx != sym->st_shndx
			     || (shndxdata != NULL && nxshndx != xshndx))
			    && (sym->st_shndx = nshndx,
				gelf_update_symshndx (shdr_info[cnt].data,
						      shndxdata,
						      destidx, sym,
						      nxshndx) == 0))
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }
		      }
		    else
		      /* This is a section symbol for a section which has
			 been removed.  */
		      assert (GELF_ST_TYPE (sym->st_info) == STT_SECTION);
		  }

		if (destidx != inner)
		  {
		    /* The size of the symbol table changed.  */
		    shdr_info[cnt].shdr.sh_size = newdata->d_size
		      = destidx * elsize;
		    any_symtab_changes = true;
		  }
		else
		  {
		    /* The symbol table didn't really change.  */
		    free (shdr_info[cnt].newsymidx);
		    shdr_info[cnt].newsymidx = NULL;
		  }
	      }
	  }

	/* If we have to, compute the offset of the section.  */
	if (shdr_info[cnt].shdr.sh_offset == 0)
	  shdr_info[cnt].shdr.sh_offset
	    = ((lastoffset + shdr_info[cnt].shdr.sh_addralign - 1)
	       & ~((GElf_Off) (shdr_info[cnt].shdr.sh_addralign - 1)));

	/* Set the section header in the new file.  */
	if (unlikely (gelf_update_shdr (scn, &shdr_info[cnt].shdr) == 0))
	  /* There cannot be any overflows.  */
	  INTERNAL_ERROR (fname);

	/* Remember the last section written so far.  */
	GElf_Off filesz = (shdr_info[cnt].shdr.sh_type != SHT_NOBITS
			   ? shdr_info[cnt].shdr.sh_size : 0);
	if (lastoffset < shdr_info[cnt].shdr.sh_offset + filesz)
	  lastoffset = shdr_info[cnt].shdr.sh_offset + filesz;
      }

  /* Adjust symbol references if symbol tables changed.  */
  if (any_symtab_changes)
    /* Find all relocation sections which use this symbol table.  */
    for (size_t cnt = 1; cnt < shdr_info.size (); ++cnt)
      {
	if (shdr_info[cnt].idx == 0)
	  /* Ignore sections which are discarded.  When we are saving a
	     relocation section in a separate debug file, we must fix up
	     the symbol table references.  */
	  continue;

	const Elf32_Word symtabidx = shdr_info[cnt].old_sh_link;
	const Elf32_Word *const newsymidx = shdr_info[symtabidx].newsymidx;
	switch (shdr_info[cnt].shdr.sh_type)
	  {
	  case SHT_REL:
	  case SHT_RELA:
	    {
	    if (no_symtab_updates (shdr_info[symtabidx]))
		break;

	    Elf_Data *d = elf_getdata (shdr_info[cnt].idx == 0
				       ? elf_getscn (debugelf, cnt)
				       : elf_getscn (newelf,
						     shdr_info[cnt].idx),
				       NULL);
	    assert (d != NULL);
	    size_t nrels = (shdr_info[cnt].shdr.sh_size
			    / shdr_info[cnt].shdr.sh_entsize);

	    if (shdr_info[cnt].shdr.sh_type == SHT_REL)
	      for (size_t relidx = 0; relidx < nrels; ++relidx)
		{
		  GElf_Rel rel_mem;
		  if (gelf_getrel (d, relidx, &rel_mem) == NULL)
		    INTERNAL_ERROR (fname);

		  size_t symidx = GELF_R_SYM (rel_mem.r_info);
		  if (newsymidx[symidx] != symidx)
		    {
		      rel_mem.r_info
			= GELF_R_INFO (newsymidx[symidx],
				       GELF_R_TYPE (rel_mem.r_info));

		      if (gelf_update_rel (d, relidx, &rel_mem) == 0)
			INTERNAL_ERROR (fname);
		    }
		}
	    else
	      for (size_t relidx = 0; relidx < nrels; ++relidx)
		{
		  GElf_Rela rel_mem;
		  if (gelf_getrela (d, relidx, &rel_mem) == NULL)
		    INTERNAL_ERROR (fname);

		  size_t symidx = GELF_R_SYM (rel_mem.r_info);
		  if (newsymidx[symidx] != symidx)
		    {
		      rel_mem.r_info
			= GELF_R_INFO (newsymidx[symidx],
				       GELF_R_TYPE (rel_mem.r_info));

		      if (gelf_update_rela (d, relidx, &rel_mem) == 0)
			INTERNAL_ERROR (fname);
		    }
		}
	    }
	    break;

	  case SHT_HASH:
	    {
	    if (no_symtab_updates (shdr_info[symtabidx]))
	      break;

	    /* We have to recompute the hash table.  */

	    assert (shdr_info[cnt].idx > 0);

	    /* The hash section in the new file.  */
	    Elf_Scn *scn = elf_getscn (newelf, shdr_info[cnt].idx);

	    /* The symbol table data.  */
	    Elf_Data *symd = elf_getdata (elf_getscn (newelf,
						      shdr_info[symtabidx].idx),
					  NULL);
	    assert (symd != NULL);

	    /* The hash table data.  */
	    Elf_Data *hashd = elf_getdata (scn, NULL);
	    assert (hashd != NULL);

	    if (shdr_info[cnt].shdr.sh_entsize == sizeof (Elf32_Word))
	      {
		/* Sane arches first.  */
		Elf32_Word *bucket = (Elf32_Word *) hashd->d_buf;

		size_t strshndx = shdr_info[symtabidx].old_sh_link;
		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					    ehdr->e_version);

		/* Adjust the nchain value.  The symbol table size
		   changed.  We keep the same size for the bucket array.  */
		bucket[1] = symd->d_size / elsize;
		Elf32_Word nbucket = bucket[0];
		bucket += 2;
		Elf32_Word *chain = bucket + nbucket;

		/* New size of the section.  */
		hashd->d_size = ((2 + symd->d_size / elsize + nbucket)
				 * sizeof (Elf32_Word));
		update_section_size (hashd, scn, debugelf, cnt);

		/* Clear the arrays.  */
		memset (bucket, '\0',
			(symd->d_size / elsize + nbucket)
			* sizeof (Elf32_Word));

		for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		     inner < symd->d_size / elsize; ++inner)
		  {
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		    assert (sym != NULL);

		    const char *name = elf_strptr (elf, strshndx,
						   sym->st_name);
		    assert (name != NULL);
		    size_t hidx = elf_hash (name) % nbucket;

		    if (bucket[hidx] == 0)
		      bucket[hidx] = inner;
		    else
		      {
			hidx = bucket[hidx];

			while (chain[hidx] != 0)
			  hidx = chain[hidx];

			chain[hidx] = inner;
		      }
		  }
	      }
	    else
	      {
		/* Alpha and S390 64-bit use 64-bit SHT_HASH entries.  */
		assert (shdr_info[cnt].shdr.sh_entsize
			== sizeof (Elf64_Xword));

		Elf64_Xword *bucket = (Elf64_Xword *) hashd->d_buf;

		size_t strshndx = shdr_info[symtabidx].old_sh_link;
		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					    ehdr->e_version);

		/* Adjust the nchain value.  The symbol table size
		   changed.  We keep the same size for the bucket array.  */
		bucket[1] = symd->d_size / elsize;
		Elf64_Xword nbucket = bucket[0];
		bucket += 2;
		Elf64_Xword *chain = bucket + nbucket;

		/* New size of the section.  */
		hashd->d_size = ((2 + symd->d_size / elsize + nbucket)
				 * sizeof (Elf64_Xword));
		update_section_size (hashd, scn, debugelf, cnt);

		/* Clear the arrays.  */
		memset (bucket, '\0',
			(symd->d_size / elsize + nbucket)
			* sizeof (Elf64_Xword));

		for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		     inner < symd->d_size / elsize; ++inner)
		  {
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		    assert (sym != NULL);

		    const char *name = elf_strptr (elf, strshndx,
						   sym->st_name);
		    assert (name != NULL);
		    size_t hidx = elf_hash (name) % nbucket;

		    if (bucket[hidx] == 0)
		      bucket[hidx] = inner;
		    else
		      {
			hidx = bucket[hidx];

			while (chain[hidx] != 0)
			  hidx = chain[hidx];

			chain[hidx] = inner;
		      }
		  }
	      }
	    }
	    break;

	  case SHT_GNU_versym:
	    {
	    /* If the symbol table changed we have to adjust the entries.  */
	    if (no_symtab_updates (shdr_info[symtabidx]))
	      break;

	    assert (shdr_info[cnt].idx > 0);

	    /* The symbol version section in the new file.  */
	    Elf_Scn *scn = elf_getscn (newelf, shdr_info[cnt].idx);

	    /* The symbol table data.  */
	    Elf_Data *symd = elf_getdata (elf_getscn (newelf, shdr_info[symtabidx].idx),
					  NULL);
	    assert (symd != NULL);

	    /* The version symbol data.  */
	    Elf_Data *verd = elf_getdata (scn, NULL);
	    assert (verd != NULL);

	    /* The symbol version array.  */
	    GElf_Half *verstab = (GElf_Half *) verd->d_buf;

	    /* Walk through the list and */
	    size_t elsize = gelf_fsize (elf, verd->d_type, 1,
					ehdr->e_version);
	    for (size_t inner = 1; inner < verd->d_size / elsize; ++inner)
	      if (newsymidx[inner] != 0)
		/* Overwriting the same array works since the
		   reordering can only move entries to lower indices
		   in the array.  */
		verstab[newsymidx[inner]] = verstab[inner];

	    /* New size of the section.  */
	    verd->d_size = gelf_fsize (newelf, verd->d_type,
				       symd->d_size
				       / gelf_fsize (elf, symd->d_type, 1,
						     ehdr->e_version),
				       ehdr->e_version);
	    update_section_size (verd, scn, debugelf, cnt);
	    }
	    break;

	  case SHT_GROUP:
	    {
	    if (no_symtab_updates (shdr_info[symtabidx]))
	      break;

	    /* Yes, the symbol table changed.
	       Update the section header of the section group.  */
	    Elf_Scn *scn = elf_getscn (newelf, shdr_info[cnt].idx);
	    GElf_Shdr shdr_mem;
	    GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	    assert (shdr != NULL);

	    shdr->sh_info = newsymidx[shdr->sh_info];

	    (void) gelf_update_shdr (scn, shdr);
	    }
	    break;
	  }
      }

  /* Finally finish the ELF header.  Fill in the fields not handled by
     libelf from the old file.  */
  newehdr = gelf_getehdr (newelf, &newehdr_mem);
  if (newehdr == NULL)
    INTERNAL_ERROR (fname);

  memcpy (newehdr->e_ident, ehdr->e_ident, EI_NIDENT);
  newehdr->e_type = ehdr->e_type;
  newehdr->e_machine = ehdr->e_machine;
  newehdr->e_version = ehdr->e_version;
  newehdr->e_entry = ehdr->e_entry;
  newehdr->e_flags = ehdr->e_flags;
  newehdr->e_phoff = ehdr->e_phoff;
  /* We need to position the section header table.  */
  const size_t offsize = gelf_fsize (elf, ELF_T_OFF, 1, EV_CURRENT);
  newehdr->e_shoff = ((shdr_info.back ().shdr.sh_offset
		       + shdr_info.back ().shdr.sh_size + offsize - 1)
		      & ~((GElf_Off) (offsize - 1)));
  newehdr->e_shentsize = gelf_fsize (elf, ELF_T_SHDR, 1, EV_CURRENT);

  /* The new section header string table index.  */
  if (likely (idx < SHN_HIRESERVE) && likely (idx != SHN_XINDEX))
    newehdr->e_shstrndx = idx;
  else
    {
      /* The index does not fit in the ELF header field.  */
      shdr_info[0].scn = elf_getscn (elf, 0);

      if (gelf_getshdr (shdr_info[0].scn, &shdr_info[0].shdr) == NULL)
	INTERNAL_ERROR (fname);

      shdr_info[0].shdr.sh_link = idx;
      (void) gelf_update_shdr (shdr_info[0].scn, &shdr_info[0].shdr);

      newehdr->e_shstrndx = SHN_XINDEX;
    }

  if (gelf_update_ehdr (newelf, newehdr) == 0)
    {
      error (0, 0, gettext ("%s: error while creating ELF header: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* We have everything from the old file.  */
  if (elf_cntl (elf, ELF_C_FDDONE) != 0)
    {
      error (0, 0, gettext ("%s: error while reading the file: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* The ELF library better follows our layout when this is not a
     relocatable object file.  */
  elf_flagelf (newelf, ELF_C_SET,
	       (ehdr->e_type != ET_REL ? ELF_F_LAYOUT : 0)
	       | (permissive ? ELF_F_PERMISSIVE : 0));

  /* Finally write the file.  */
  if (elf_update (newelf, ELF_C_WRITE) == -1)
    {
      error (0, 0, gettext ("while writing '%s': %s"),
	     fname, elf_errmsg (-1));
      result = 1;
    }

  goto fail_close;
}

int
main (int argc, char *argv[])
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  elf_version (EV_CURRENT);

  /* We expect exactly one non-option parameter.  */
  if (unlikely (remaining + 1 != argc))
    {
      fputs (gettext ("Invalid number of parameters.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE, program_invocation_short_name);
      exit (1);
    }

  const char *const fname = argv[remaining];

  int fd = open (fname, O_RDONLY);
  struct stat64 st;
  fstat64 (fd, &st);
  Elf *elf = elf_begin (fd, ELF_C_READ,	NULL);
  handle_elf (fd, elf, fname, st.st_mode & ACCESSPERMS, NULL);
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "dwarfcmp (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2009");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *c __attribute__ ((unused)),
	   struct argp_state *st __attribute__ ((unused)))
{
  switch (key)
    {
    case 'q':
      quiet = true;
      break;

    case 'i':
      missing_ok = true;
      break;

    case 'T':
      ++test_writer;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
