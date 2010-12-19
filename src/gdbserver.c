/* Provide gdbserver network interface from specified image.
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

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <system.h>
#include <netdb.h>
#include <stdlib.h>
#include <libintl.h>
#include <string.h>
#include <unistd.h>
#include <gelf.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/param.h>

/* FIXME: regs should use GDB XML arch descriptor instead!  */
#include <sys/user.h>
#include <sys/procfs.h>

/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

#define memcmpstring(mem, string) memcmp ((mem), (string), strlen (string))
#define ARRAY_SIZE(x) (sizeof (x) / sizeof (*(x)))

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Locate source files and line information for ADDRs (in a.out by default).");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("<port> <corefile>");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static const struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};

/* Returns file descriptor of an accepted TCP connection.  */

static int
open_socket (const char *ports)
{
  const struct addrinfo hints =
    {
      ai_flags : AI_PASSIVE,
      ai_family : AF_UNSPEC,
      ai_socktype : SOCK_STREAM,
    };
  struct addrinfo *addrinfop, *rp;
  int i, sock, sock2;
  struct sockaddr sockaddr;
  socklen_t sockaddr_len = sizeof (sockaddr);

  if (ports == NULL)
    error (EXIT_FAILURE, 0, gettext ("Port parameter required"));

  i = getaddrinfo (NULL, ports, &hints, &addrinfop);
  if (i != 0)
    error (EXIT_FAILURE, 0, gettext ("Error parsing port \"%s\": %s"),
	   ports, gai_strerror (i));

  for (rp = addrinfop; rp != NULL; rp = rp->ai_next)
    {
      sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sock == -1)
	continue;

      i = 1;
      /* Errors ignored.  */
      setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof (i));

      if (bind (sock, rp->ai_addr, rp->ai_addrlen) == 0)
	break;

      if (close (sock) != 0)
	error (EXIT_FAILURE, errno, gettext ("Could not close the port"));
    }
  if (rp == NULL)
    error (EXIT_FAILURE, errno, gettext ("Could not bind to the port"));
  freeaddrinfo (addrinfop);

  i = listen (sock, 1);
  if (i != 0)
    error (EXIT_FAILURE, errno, gettext ("Could not listen on the port"));

  sock2 = accept (sock, &sockaddr, &sockaddr_len);
  if (sock2 == -1)
    error (EXIT_FAILURE, errno, gettext ("Could not accept on the port"));

  i = close (sock);
  if (i != 0)
    error (EXIT_FAILURE, errno, gettext ("Could not close the port"));

  return sock2;
}

static void
xread (int fd, void *buf, size_t count)
{
  int i;
  
  errno = 0;
  i = read (fd, buf, count);
  if (i < 0 || (size_t) i != count)
    error (EXIT_FAILURE, errno, gettext ("Error reading data"));
}

static void
xwrite (int fd, const void *buf, size_t count)
{
  int i;
  
  errno = 0;
  i = write (fd, buf, count);
  if (i < 0 || (size_t) i != count)
    error (EXIT_FAILURE, errno, gettext ("Error writing data"));
}

static unsigned
xreadc (int fd)
{
  unsigned char uc;

  xread (fd, &uc, 1);

  return uc;
}

static void
xwritec (int fd, char ch)
{
  xwrite (fd, &ch, 1);
}

static void
xfputc (char ch, FILE *file)
{
  int got = fputc (ch, file);

  if (got != ch)
    error (EXIT_FAILURE, errno, gettext ("Error writing byte 0x%02x"),
	   (unsigned) (unsigned char) ch);
}

static void
xfwrite (const void *buf, size_t size, FILE *file)
{
  size_t got = fwrite (buf, 1, size, file);

  if (got != size)
    error (EXIT_FAILURE, errno, gettext ("Error writing %zu bytes"), size);
}

static void
xfclose (FILE *file)
{
  int got = fclose (file);

  if (got != 0)
    error (EXIT_FAILURE, errno, gettext ("Error closing stream"));
}

static void
xfputs (const char *cs, FILE *file)
{
  int got = fputs (cs, file);

  if (got < 0)
    error (EXIT_FAILURE, errno, gettext ("Error writing string '%s'"), cs);
}

static void
xfprintf (FILE *file, const char *fmt, ...)
{
  va_list ap;
  int i;

  errno = 0;
  va_start (ap, fmt);
  i = vfprintf (file, fmt, ap);
  va_end (ap);

  if (i <= 0)
    {
      if (errno == 0)
	errno = EIO;
      error (EXIT_FAILURE, errno,
	     gettext ("Error printing format string '%s'"), fmt);
    }
}

/* FIXME: regs should use GDB XML arch descriptor instead!  */

struct gdbreg
  {
    unsigned short gdb_size, gdb_offs, usr_offs;
  };

/* Stolen from Oleg Nesterov's ugdb and modified.  */
/* generated from gdb/regformats/reg-x86-64-linux.dat */
static const struct gdbreg gdb_regmap_64[] =
  {
#define GEN__(mem) usr_offs : offsetof(struct user_regs_struct, mem)
    { .gdb_size =  8, .gdb_offs =   0, GEN__(rax) },
    { .gdb_size =  8, .gdb_offs =   8, GEN__(rbx) },
    { .gdb_size =  8, .gdb_offs =  16, GEN__(rcx) },
    { .gdb_size =  8, .gdb_offs =  24, GEN__(rdx) },
    { .gdb_size =  8, .gdb_offs =  32, GEN__(rsi) },
    { .gdb_size =  8, .gdb_offs =  40, GEN__(rdi) },
    { .gdb_size =  8, .gdb_offs =  48, GEN__(rbp) },
    { .gdb_size =  8, .gdb_offs =  56, GEN__(rsp) },
    { .gdb_size =  8, .gdb_offs =  64, GEN__(r8) },
    { .gdb_size =  8, .gdb_offs =  72, GEN__(r9) },
    { .gdb_size =  8, .gdb_offs =  80, GEN__(r10) },
    { .gdb_size =  8, .gdb_offs =  88, GEN__(r11) },
    { .gdb_size =  8, .gdb_offs =  96, GEN__(r12) },
    { .gdb_size =  8, .gdb_offs = 104, GEN__(r13) },
    { .gdb_size =  8, .gdb_offs = 112, GEN__(r14) },
    { .gdb_size =  8, .gdb_offs = 120, GEN__(r15) },
    { .gdb_size =  8, .gdb_offs = 128, GEN__(rip) },
    { .gdb_size =  4, .gdb_offs = 136, GEN__(eflags) },
    { .gdb_size =  4, .gdb_offs = 140, GEN__(cs) },
    { .gdb_size =  4, .gdb_offs = 144, GEN__(ss) },
    { .gdb_size =  4, .gdb_offs = 148, GEN__(ds) },
    { .gdb_size =  4, .gdb_offs = 152, GEN__(es) },
    { .gdb_size =  4, .gdb_offs = 156, GEN__(fs) },
    { .gdb_size =  4, .gdb_offs = 160, GEN__(gs) },
    { .gdb_size =  8, .gdb_offs = 536, GEN__(orig_rax) },
#undef GEN__
  };

struct core
  {
    Elf *elf;
    int fd;
    const char *fname;
    GElf_Ehdr ehdr;
    size_t phdr_count;
    GElf_Phdr *phdr;
    /* FIXME: regs should use GDB XML arch descriptor instead!  */
    unsigned char regs[536 + 8];
    int signo;
    unsigned char *auxv;
    size_t auxv_size;
  };

static struct core *
core_open (const char *fname)
{
  struct core *core;
  int fd;
  Elf *elf;
  
  if (fname == NULL)
    error (EXIT_FAILURE, 0, gettext ("Core file parameter required"));

  fd = open (fname, O_RDONLY);
  if (fd == -1)
    error (EXIT_FAILURE, errno, gettext ("Cannot open `%s'"), fname);

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, errno, gettext ("%s: File format not recognized"),
	   fname);

  if (elf_kind (elf) != ELF_K_ELF)
    error (EXIT_FAILURE, 0, gettext ("Unsupport ELF kind of `%s'"), fname);

  core = xcalloc (1, sizeof (*core));
  core->elf = elf;
  core->fd = fd;
  core->fname = fname;

  return core;
}

static void
core_read_note (struct core *core, GElf_Phdr *phdr)
{
  Elf_Data *data;
  size_t offset;
  const unsigned char *data_cus;

  data = elf_getdata_rawchunk (core->elf, phdr->p_offset, phdr->p_filesz,
			       ELF_T_NHDR);
  if (data == NULL)
    error (EXIT_FAILURE, errno,
           gettext ("Error reading note at core file offset 0x%llx"),
	   (unsigned long long) phdr->p_offset);
  data_cus = data->d_buf;

  offset = 0;
  while (offset < data->d_size)
    {
      GElf_Nhdr nhdr;
      size_t name_offset;
      size_t desc_offset;
      size_t offset2;

      offset2 = gelf_getnote (data, offset, &nhdr, &name_offset,
			      &desc_offset);
      if (offset2 <= 0)
	break;

      switch (nhdr.n_type)
	{
	case NT_PRSTATUS:
	  {
	    const struct gdbreg *gdbreg;
	    const struct elf_prstatus *prstatus;
	    
	    prstatus = (void *) &data_cus[desc_offset];

	    core->signo = prstatus->pr_cursig;

	    for (gdbreg = gdb_regmap_64;
		 gdbreg < gdb_regmap_64 + ARRAY_SIZE (gdb_regmap_64);
		 gdbreg++)
	      memcpy (&core->regs[gdbreg->gdb_offs],
		      &((const char *) prstatus->pr_reg)[gdbreg->usr_offs],
		      gdbreg->gdb_size);
	  }
	  break;

	case NT_AUXV:
	  free (core->auxv);
	  core->auxv = xmalloc (nhdr.n_descsz);
	  core->auxv_size = nhdr.n_descsz;
	  memcpy (core->auxv, &data_cus[desc_offset], core->auxv_size);
	  break;
	}

      offset = offset2;
    }
  if (offset != data->d_size)
    error (EXIT_FAILURE, errno,
	   gettext ("Error reading note 0x%llx at core segment 0x%llx"),
	   (unsigned long long) offset, (unsigned long long) phdr->p_offset);
}

static void
core_read (struct core *core)
{
  Elf *elf = core->elf;
  GElf_Ehdr *ehdr;
  size_t phdri;
  
  ehdr = gelf_getehdr (elf, &core->ehdr);
  if (ehdr == NULL)
    error (EXIT_FAILURE, errno, gettext ("Error reading ELF header of `%s'"),
	   core->fname);
  assert (ehdr == &core->ehdr);

  if (ehdr->e_type != ET_CORE)
    error (EXIT_FAILURE, errno,
           gettext ("%s: Only ELF core files are supported"), core->fname);

  core->phdr = xmalloc (sizeof (*core->phdr) * ehdr->e_phnum);
  core->phdr_count = 0;
  for (phdri = 0; phdri < ehdr->e_phnum; phdri++)
    {
      GElf_Phdr *phdr_mem = &core->phdr[core->phdr_count];
      GElf_Phdr *phdr;

      phdr = gelf_getphdr (elf, phdri, phdr_mem);
      if (phdr == NULL)
	error (EXIT_FAILURE, errno,
	       gettext ("Error reading ELF program header %zu of `%s'"),
	       phdri, core->fname);
      assert (phdr == phdr_mem);

      switch (phdr->p_type)
	{
	case PT_LOAD:
	  core->phdr_count++;
	  break;

	case PT_NOTE:
	  core_read_note (core, phdr);
	  break;
	}
    }
}

static void
core_close (struct core *core)
{
  if (elf_end (core->elf) != 0)
    error (EXIT_FAILURE, errno, gettext ("Cannot close ELF `%s'"),
	   core->fname);

  if (close (core->fd) != 0)
    error (EXIT_FAILURE, errno, gettext ("Error closing `%s'"), core->fname);

  free (core->phdr);
  free (core);
}

static void
xclose (int fd)
{
  int got = close (fd);

  if (got != 0)
    error (EXIT_FAILURE, errno, gettext ("Error closing core file"));
}

/* FIXME: Associate it with SOCK.  */
static enum
  {
    NOACK_NO = 0,
    NOACK_YES = 1,
    NOACK_LAST,
  }
sock_noack = NOACK_NO;

/* BUF and *LENP contain only the net packet data.  */

static void
packet_compress (char *buf, size_t *lenp)
{
  char *d, lastch;
  size_t pos, run;

  if (*lenp < 4)
    return;

  lastch = buf[0];
  d = &buf[1];
  pos = 1;
  while (pos < *lenp - 2)
    {
      if (buf[pos] != lastch || buf[pos + 1] != lastch
	  || buf[pos + 2] != lastch)
	{
	  lastch = *d++ = buf[pos++];
	  continue;
	}

      /* `qXfer:auxv:read' requires an 8bit line anyway.  */
      for (run = pos + 3; run < MIN (*lenp, pos + (255 - 29)); run++)
	if (buf[run] != lastch)
	  break;
      
      *d++ = '*';
      *d++ = run - pos + 29;
      pos = run;
    }
  while (pos < *lenp)
    *d++ = buf[pos++];

  *lenp = d - buf;
}

/* BUF and LEN contain the full: $packet#xs  */

static void
packet_send (int sock, char *buf, size_t len)
{
  const unsigned char *cus_start = (void *) &buf[1], *cus;
  unsigned char xsum;
  int i;

  assert (len >= 1 + 3);
  assert (buf[0] == '$');
  assert (buf[len - 3] == '#');

  len -= 1 + 3;
  packet_compress (&buf[1], &len);
  len += 1 + 3;
  buf[len - 3] = '#';

  xsum = 0;
  for (cus = cus_start; (char *) cus < buf + len - 3; cus++)
    xsum += *cus;
  sprintf (&buf[len - 2], "%02X", (unsigned) xsum);

  xwrite (sock, buf, len);

  if (sock_noack != NOACK_YES)
    {
      i = xreadc (sock);
      if (i != '+')
	error (EXIT_FAILURE, 0,
	       gettext ("Packet confirmation is not '+' but 0x%02x"),
	       (unsigned) i);
    }
  if (sock_noack == NOACK_LAST)
    sock_noack = NOACK_YES;
}

static void
packet_hexdump (FILE *out, const void *buf, size_t size)
{
  const unsigned char *cus_start, *cus;

  cus_start = buf;
  for (cus = cus_start; cus < cus_start + size; cus++)
    xfprintf (out, "%02X", (unsigned) *cus);
}

#define BUF_ALLOC_RESERVE 0x100

/* Parses the format: $data#xs  */

static char *
read_packet (int sock, size_t *len_return)
{
  static char *buf;
  static size_t buf_allocated;
  size_t buf_len, sizet;
  int i, n;
  unsigned char sum_made;
  unsigned sum_found;
  int start_seen = 0;
  char *hashp;

  buf_len = 0;
  for (;;)
    {
      ssize_t got;
  
      if (!start_seen && buf_len)
	{
	  if (buf[0] != '$')
	    error (EXIT_FAILURE, 0,
		   gettext ("Packet start is not '$' but 0x%02x"),
		   (unsigned) (unsigned char) buf[0]);
	  start_seen = 1;
	}

      if (start_seen)
	{
	  /* Wait till '#' and the two bytes of checksum are read in.  */
	  hashp = memchr (&buf[1], '#', buf_len - 1);
	  if (hashp && buf_len >= (size_t) (hashp - buf) + 3)
	    break;
	}

      /* Read in more data.  */

      i = fcntl (sock, F_SETFL, O_NONBLOCK);
      if (i != 0)
	error (EXIT_FAILURE, errno,
	       gettext ("Cannot set socket to non-blocking mode"));

      if (buf_len + BUF_ALLOC_RESERVE > buf_allocated)
	{
	  buf_allocated = 2 * buf_allocated;
	  if (buf_allocated == 0)
	    buf_allocated = BUF_ALLOC_RESERVE;
	  buf = xrealloc (buf, buf_allocated);
	}
      errno = 0;
      got = read (sock, &buf[buf_len], buf_allocated - buf_len);
      if (got == 0)
	error (EXIT_FAILURE, errno,
	       gettext ("Socket connection close by the GDB client"));
      if (got < 0 && (errno != EAGAIN && errno != EWOULDBLOCK))
	error (EXIT_FAILURE, errno,
	       gettext ("Error reading from the socket"));

      i = fcntl (sock, F_SETFL, 0);
      if (i != 0)
	error (EXIT_FAILURE, errno,
	       gettext ("Cannot reset socket from non-blocking mode"));

      if (got < 0)
	{
	  buf[buf_len] = xreadc (sock);
	  got = 1;
	}

      buf_len += got;
    }

  sum_made = 0;
  for (sizet = 1; sizet < (size_t) (hashp - buf); sizet++)
    sum_made += (unsigned char) buf[sizet];

  hashp[3] = 0;

  i = sscanf (&hashp[1], "%x%n", &sum_found, &n);
  if (i != 1 || n != 2)
    error (EXIT_FAILURE, 0, gettext ("Error reading checksum from `%s'"),
	   &hashp[1]);
  assert (sum_found == (unsigned char) sum_found);

  if (sum_made != sum_found)
    error (EXIT_FAILURE, 0,
	   gettext ("Invalid checksum (calculated 0x%02x, found 0x%02x"),
	   (unsigned) sum_made, (unsigned) sum_found);

  if (sock_noack == NOACK_NO)
    xwritec (sock, '+');

  if (len_return)
    *len_return = hashp - &buf[1];
  *hashp = 0;

  return &buf[1];
}

static void
command_read_memory (GElf_Addr mem_addr, GElf_Addr mem_len, struct core *core,
		     FILE *packet_out)
{
  GElf_Phdr *phdr;
  int first = 1;

  phdr = core->phdr;
  while (mem_len && phdr < core->phdr + core->phdr_count)
    {
      size_t size;
      Elf_Data *data;

      if (mem_addr < phdr->p_vaddr
	  || mem_addr >= phdr->p_vaddr + phdr->p_filesz)
	{
	  phdr++;
	  continue;
	}

      size = MIN (mem_len, phdr->p_vaddr + phdr->p_filesz - mem_addr);

      /* FIXME: libelf never frees such DATA chunk.  */
      data = elf_getdata_rawchunk (core->elf,
				   phdr->p_offset + mem_addr - phdr->p_vaddr,
				   size, ELF_T_BYTE);
      if (data == NULL)
	{
	  if (first)
	    xfprintf (packet_out, "E%02X", errno);
	  return;
	}
      assert (data->d_size == size);

      packet_hexdump (packet_out, data->d_buf, size);

      mem_addr += size;
      mem_len -= size;
      first = 0;
      phdr = core->phdr;
    }
}

static void
command_read_registers (struct core *core, FILE *packet_out)
{
  packet_hexdump (packet_out, core->regs, sizeof (core->regs));
}

/* Returns whether we should continue with next command.  */

static int
read_command (int sock, struct core *core)
{
  size_t packet_len;
  char *packet = read_packet (sock, &packet_len);
  char cmd = packet[0];
  unsigned long long ull1, ull2;
  GElf_Addr addr1, addr2;
  int i, n;
  FILE *out;
  char *out_mem;
  size_t out_len;

  out = open_memstream (&out_mem, &out_len);
  if (out == NULL)
    error (EXIT_FAILURE, errno, gettext ("Error allocating output packet"));

  xfputc ('$', out);

  switch (cmd)
    {
    /* Read bytes of memory.  */
    case 'm':
      i = sscanf (&packet[1], "%llx,%llx%n", &ull1, &ull2, &n);
      addr1 = ull1;
      addr2 = ull2;
      if (i != 2 || (size_t) n != strlen (&packet[1]) || addr1 != ull1
	  || addr2 != ull2)
	error (EXIT_FAILURE, 0, gettext ("Error parsing packet `%s'"),
	       packet);
      command_read_memory (addr1, addr2, core, out);
      break;

    /* General query (`q').  */
    case 'q':
      if (memcmpstring (&packet[1], "Supported") == 0
	  && (packet[1 + strlen ("Supported")] == 0
	      || packet[1 + strlen ("Supported")] == ':'))
	{
	  /* `qSupported [:GDBFEATURE [;GDBFEATURE]... ]'
	     Tell the remote stub about features supported by GDB.  */

	  xfputs ("PacketSize=4000;QStartNoAckMode+;qXfer:memory-map:read+;"
	          "qXfer:auxv:read+", out);
	  break;
	}
      if (memcmpstring (&packet[1], "Xfer:memory-map:read::0,") == 0)
	{
	  /* "cache" requires a non FSF GDB patch.  For FSF GDB use:
	     (gdb) delete mem 
	     (gdb) mem 0 -1 ro cache  */

	  xfputs ("l\
<?xml version=\"1.0\"?>\n\
<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\"\n\
\t\"http://sourceware.org/gdb/gdb-memory-map.dtd\">\n\
<memory-map>\n\
\t<memory type=\"rom\" start=\"0\" length=\"-1\">\n\
\t\t<property name=\"cache\">1</property>\n\
\t</memory>\n\
</memory-map>\n\
",
		  out);
	  break;
	}
      if (memcmpstring (&packet[1], "Xfer:auxv:read::0,") == 0)
	{
	  xfputc ('l', out);
	  xfwrite (core->auxv, core->auxv_size, out);
	  break;
	}
      /* Not supported - empty response.  */
      break;

    /* General set (`Q').  */
    case 'Q':
      if (strcmp (&packet[1], "StartNoAckMode") == 0)
	{
	  sock_noack = NOACK_LAST;
	  xfputs ("OK", out);
	  break;
	}
      /* Not supported - empty response.  */
      break;

    /* Read general registers.  */
    case 'g':
      if (packet[1] != 0)
	{
	  /* Not supported - empty response.  */
	  break;
	}
      command_read_registers (core, out);
      break;

    /* Kill request.  */
    case 'k':
      xfclose (out);
      free (out_mem);
      return 0;

    /* Continue.  */
    case 'c':
    /* Single step.  */
    case 's':
    /* Continue with signal SIG (hex signal number).  */
    case 'C':
    /* Step with signal.  */
    case 'S':
    case '?':
      /* The program received signal.  */
      xfprintf (out, "S%02X", core->signo);
      break;

    /* Write general registers.  */
    case 'G':
    /* Write bytes of memory.  */
    case 'M':
      xfputs ("E01", out);
      break;

    default:;
      /* Not supported - empty response.  */
    }

  xfputs ("#??", out);
  xfclose (out);
  packet_send (sock, out_mem, out_len);
  free (out_mem);

  return 1;
}

int
main (int argc, char *argv[])
{
  int remaining;
  int sock, i;
  struct core *core;

  /* Make memory leak detection possible.  */
  mtrace ();

  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  This includes opening the modules.  */
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  (void) elf_version (EV_CURRENT);

  core = core_open (remaining + 2 != argc ? NULL : argv[remaining + 1]);
  core_read (core);

  sock = open_socket (remaining + 2 != argc ? NULL : argv[remaining + 0]);

  /* FIXME: Why?  */
  i = xreadc (sock);
  if (i != '+')
    error (EXIT_FAILURE, 0, gettext ("Initial `+' found as 0x%02X"),
	   (unsigned) i);

  while (read_command (sock, core));

  core_close (core);

  xclose (sock);

  return EXIT_SUCCESS;
}

/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "elfutils gdbserver (%s) %s\n", PACKAGE_NAME,
	   PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2010");
}

/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg __attribute__((unused)),
	   struct argp_state *state __attribute__((unused)))
{
  switch (key)
    {
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

#include "debugpred.h"
