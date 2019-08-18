/* Debuginfo file server.
   Copyright (C) 2019 Red Hat, Inc.
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


/* cargo-cult from libdwfl linux-kernel-modules.c */
/* In case we have a bad fts we include this before config.h because it
   can't handle _FILE_OFFSET_BITS.
   Everything we need here is fine if its declarations just come first.
   Also, include sys/types.h before fts. On some systems fts.h is not self
   contained. */
#ifdef BAD_FTS
  #include <sys/types.h>
  #include <fts.h>
#endif

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

extern "C" {
#include "printversion.h"
}

#include "dbgserver-client.h"
#include <dwarf.h>

#include <argp.h>
#include <unistd.h>
#include <stdlib.h>
#include <error.h>
#include <libintl.h>
#include <locale.h>
#include <regex.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>


/* If fts.h is included before config.h, its indirect inclusions may not
   give us the right LFS aliases of these functions, so map them manually.  */
#ifdef BAD_FTS
  #ifdef _FILE_OFFSET_BITS
    #define open open64
    #define fopen fopen64
  #endif
#else
  #include <sys/types.h>
  #include <fts.h>
#endif

#include <cstring>
#include <vector>
#include <set>
#include <string>
#include <iostream>
#include <ostream>
#include <sstream>
// #include <algorithm>
using namespace std;

#include <gelf.h>
#include <libdwelf.h>

#include <microhttpd.h>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>
#include <sqlite3.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

#ifndef _
# define _(str) gettext (str)
#endif


// Roll this identifier for every sqlite schema incompatiblity.
#define BUILDIDS "buildids4"

static const char DBGSERVER_SQLITE_DDL[] =
  "pragma foreign_keys = on;\n"
  "pragma synchronous = 0;\n" // disable fsync()s - this cache is disposable across a machine crash
  "pragma journal_mode = wal;\n" // https://sqlite.org/wal.html

  /* Normalized tables to represent general buildid-to-file/subfile mapping. */
  "create table if not exists " BUILDIDS "_files (\n"
  "        id integer primary key not null,\n"
  "        name text unique not null);\n"
  "create table if not exists " BUILDIDS "_buildids (\n"
  "        id integer primary key not null,\n"
  "        hex text unique not null);\n"
  "create table if not exists " BUILDIDS "_norm (\n"
  "        buildid integer,\n"
  "        artifacttype text\n"                           // -- D(ebug) E(xecutable) S(source)
  "            check (artifacttype IS NULL or artifacttype IN ('D', 'E', 'S')),\n"
  "        artifactsrc integer\n"                         //                         DWARF /path/to/source
  "            check (artifacttype NOT IN ('S') OR artifactsrc is not null),\n"
  "        mtime integer,\n"                               // -- epoch timestamp when we last found this source0
  "        sourcetype text(1) not null\n"
  "            check (sourcetype IN ('F', 'R')),\n"        // -- as per --source-TYPE single-char code\n"
  "        source0 integer not null,\n"
  "        source1 integer,\n"
  "        foreign key (artifactsrc) references " BUILDIDS "_files(id) on update cascade on delete cascade,\n"
  "        foreign key (source0) references " BUILDIDS "_files(id) on update cascade on delete cascade,\n"
  "        foreign key (buildid) references " BUILDIDS "_buildids(id) on update cascade on delete cascade,\n"
  "        foreign key (source1) references " BUILDIDS "_files(id) on update cascade on delete cascade,\n"
  "        unique (buildid, artifacttype, artifactsrc, sourcetype, source0) on conflict replace);\n"
  /* and now for a FULL OUTER JOIN emulation */
  "create view if not exists " BUILDIDS "  as select\n"
  "        b.hex as buildid, n.artifacttype, f3.name as artifactsrc, n.mtime, n.sourcetype, f1.name as source0, f2.name as source1\n"
  "        from " BUILDIDS "_buildids b, " BUILDIDS "_norm n, " BUILDIDS "_files f1, " BUILDIDS "_files f2, " BUILDIDS "_files f3\n"
  "        where b.id = n.buildid and f1.id = n.source0 and f2.id = n.source1 and f3.id = n.artifactsrc\n"
  "union all select\n"
  "        b.hex as buildid, n.artifacttype, null, n.mtime, n.sourcetype, f1.name as source0, f2.name as source1\n"
  "        from " BUILDIDS "_buildids b, " BUILDIDS "_norm n, " BUILDIDS "_files f1, " BUILDIDS "_files f2\n"
  "        where b.id = n.buildid and f1.id = n.source0 and f2.id = n.source1 and n.artifactsrc is null\n"
  "union all select\n"
  "        b.hex as buildid, n.artifacttype, f3.name, n.mtime, n.sourcetype, f1.name as source0, null\n"
  "        from " BUILDIDS "_buildids b, " BUILDIDS "_norm n, " BUILDIDS "_files f1, " BUILDIDS "_files f3\n"
  "        where b.id = n.buildid and f1.id = n.source0 and n.source1 is null and f3.id = n.artifactsrc\n"
  "union all select\n"
  "        b.hex as buildid, n.artifacttype, null, n.mtime, n.sourcetype, f1.name as source0, null\n"
  "        from " BUILDIDS "_buildids b, " BUILDIDS "_norm n, " BUILDIDS "_files f1\n"
  "        where b.id = n.buildid and f1.id = n.source0 and n.source1 is null and n.artifactsrc is null\n"
  "union all select\n" // negative hit
  "        null, null, null, n.mtime, n.sourcetype, f1.name as source0, null\n"
  "        from " BUILDIDS "_norm n, " BUILDIDS "_files f1\n"
  "        where n.buildid is null and f1.id = n.source0;\n"

  "create index if not exists " BUILDIDS "_idx1 on " BUILDIDS "_norm (buildid, artifacttype);\n"
  "create index if not exists " BUILDIDS "_idx2 on " BUILDIDS "_norm (mtime, sourcetype, source0);\n" 
  
  /* BUILDIDS semantics:

     buildid  atype/asrc  mtime  stype  source0  source1 
     $BUILDID D/E         $TIME  F      $FILE            -- normal hit: executable or debuinfo file
     $BUILDID S $SRC      $TIME  F      $FILE            -- normal hit: source file (FILE actual location, SRC dwarf)
     $BUILDID D/E         $TIME  R      $RPM     $FILE   -- normal hit: executable or debuinfo file in rpm RPM  file FILE
     $BUILDID S $SRC      $TIME  R      $RPM     $FILE   -- normal hit: source file (RPM rpm, FILE content, SRC dwarf)
                          $TIME  F/R    $FILE            -- negative hit: bad file known to be unrescanworthy at $TIME
     \-----------/               \----------/  UNIQUE
  */

  /* Denormalized table for source be-on-the-lookout mappings.  Denormalized because it's a temporary table:
     in steady state it's empty. */
  
  "create table if not exists " BUILDIDS "_bolo (\n"
  "        buildid text not null,\n"
  "        srcname text not null,\n"
  "        sourcetype text(1) not null\n"
  "            check (sourcetype IN ('F', 'R')),\n"        // -- as per --source-TYPE single-char code\n"
  "        dirname text not null,\n"
  "        unique (buildid, srcname, sourcetype, dirname) on conflict ignore);\n"

  "create index if not exists " BUILDIDS "_bolo_idx1 on " BUILDIDS "_bolo (sourcetype, dirname);\n"
  /*
     BUILDIDS_bolo semantics:

     $BUILDID $SRC          F      $DIR     -- source BOLO: recently looking for dwarf SRC mentioned under fts-$DIR
     $BUILDID $SRC          R      $DIR     -- source BOLO: recently looking for dwarf SRC mentioned in RPM under fts-$DIR
  */

  /* Denormalized table for rpm found-files mappings.  Denormalized because it's a temporary table:
     in steady state it's empty. */
  
  "create table if not exists " BUILDIDS "_rfolo (\n"
  "        source0 text not null,\n" // rpm file name
  "        mtime integer not null,\n" // rpm file mtime
  "        source1 text not null,\n" // rpm content file name
  "        dirname text not null,\n"
  "        unique (source0, source1, dirname) on conflict replace);\n"

  "create index if not exists " BUILDIDS "_rfolo_idx1 on " BUILDIDS "_rfolo (source0, dirname);\n"

  
// schema change history & garbage collection
//
// buildids4: introduce rpmfile SOLO
  "" // <<< we are here
// buildids3*: split out srcfile BOLO
  "drop table if exists buildids3_norm;\n"
  "drop table if exists buildids3_files;\n"
  "drop table if exists buildids3_buildids;\n"
  "drop table if exists buildids3_bolo;\n"
  "drop view if exists buildids3;\n"
// buildids2: normalized buildid and filenames into interning tables;
  "drop table if exists buildids2_norm;\n"
  "drop table if exists buildids2_files;\n"
  "drop table if exists buildids2_buildids;\n"  
  "drop view if exists buildids2;\n"
  // buildids1: made buildid and artifacttype NULLable, to represent cached-negative
//           lookups from sources, e.g. files or rpms that contain no buildid-indexable content
  "drop table if exists buildids1;\n"
// buildids: original
  "drop table if exists buildids;\n"
  ;


/*
  ISSUES:
  - delegated server: recursion/loop; Via: header processing
  https://blog.cloudflare.com/preventing-malicious-request-loops/
  - access control ===>> delegate to reverse proxy
  - running real server for rhel/rhsm probably unnecessary
  (use subscription-delegation)
  - need a thread to garbage-collect old buildid_norm / _buildid / _files entries?
  - inotify based file scanning

  see also:
  https://github.com/NixOS/nixos-channel-scripts/blob/master/index-debuginfo.cc
  https://github.com/edolstra/dwarffs
*/


/* Name and version of program.  */
/* ARGP_PROGRAM_VERSION_HOOK_DEF = print_version; */ // not this simple for C++

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
  {
   { NULL, 0, NULL, 0, N_("Sources:"), 1 },
   { "source-files", 'F', "PATH", 0, N_("Scan ELF/DWARF files under given directory."), 0 },
   { "source-rpms", 'R', "PATH", 0, N_("Scan RPM files under given directory."), 0 },
   //  { "source-rpms-yum", 0, "SECONDS", 0, N_("Try fetching missing RPMs from yum."), 0 },
   // "source-rpms-koji"      ... no can do, not buildid-addressable
   // http traversal for rpm downloading?
   // "source-oci-imageregistry"  ... 
  
   { NULL, 0, NULL, 0, N_("Options:"), 2 },
   { "rescan-time", 't', "SECONDS", 0, N_("Number of seconds to wait between rescans."), 0 },
   { "port", 'p', "NUM", 0, N_("HTTP port to listen on."), 0 },
   { "database", 'd', "FILE", 0, N_("Path to sqlite database."), 0 },
   { "verbose", 'v', NULL, 0, N_("Increase verbosity."), 0 },
    
   { NULL, 0, NULL, 0, NULL, 0 }
  };

/* Short description of program.  */
static const char doc[] = N_("Serve debuginfo-related content across HTTP.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[--source-TYPE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
  {
   options, parse_opt, args_doc, doc, NULL, NULL, NULL
  };


static string db_path;
static sqlite3 *db;
static unsigned verbose;
static volatile sig_atomic_t interrupted = 0;
static unsigned http_port;
static unsigned rescan_s = 300;
static set<string> source_file_paths;
static vector<pthread_t> source_file_scanner_threads;
static set<string> source_rpm_paths;
static vector<pthread_t> source_rpm_scanner_threads;



/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'v': verbose ++; break;
    case 'd': db_path = string(arg); break;
    case 'p': http_port = atoi(arg); break;
    case 'F': source_file_paths.insert(string(arg)); break;
    case 'R': source_rpm_paths.insert(string(arg)); break;
    case 't': rescan_s = atoi(arg); break;
      // case 'h': argp_state_help (state, stderr, ARGP_HELP_LONG|ARGP_HELP_EXIT_OK);
    default: return ARGP_ERR_UNKNOWN;
    }

  return 0;
}


////////////////////////////////////////////////////////////////////////


// represent errors that may get reported to an ostream and/or a libmicrohttpd connection

struct reportable_exception
{
  int code;
  string message;

  reportable_exception(int code, const string& message): code(code), message(message) {}
  reportable_exception(const string& message): code(503), message(message) {}
  reportable_exception(): code(503), message() {}
  
  void report(ostream& o) const; // defined under obatched() class below
  
  int mhd_send_response(MHD_Connection* c) const {
    MHD_Response* r = MHD_create_response_from_buffer (message.size(),
                                                       (void*) message.c_str(),
                                                       MHD_RESPMEM_MUST_COPY);
    int rc = MHD_queue_response (c, code, r);
    MHD_destroy_response (r);
    return rc;
  }
};


struct sqlite_exception: public reportable_exception
{
  sqlite_exception(int rc, const string& msg):
    reportable_exception(string("sqlite3 error: ") + msg + ": " + string(sqlite3_errstr(rc) ?: "?")) {}
};

struct libc_exception: public reportable_exception
{
  libc_exception(int rc, const string& msg):
    reportable_exception(string("libc error: ") + msg + ": " + string(strerror(rc) ?: "?")) {}
};


struct archive_exception: public reportable_exception
{
  archive_exception(const string& msg):
    reportable_exception(string("libarchive error: ") + msg) {}
  archive_exception(struct archive* a, const string& msg):
    reportable_exception(string("libarchive error: ") + msg + ": " + string(archive_error_string(a) ?: "?")) {}
};


struct elfutils_exception: public reportable_exception
{
  elfutils_exception(int rc, const string& msg):
    reportable_exception(string("elfutils error: ") + msg + ": " + string(elf_errmsg(rc) ?: "?")) {}
};




// Lightweight wrapper for pthread_mutex_t
struct my_lock_t
{
private:
  pthread_mutex_t _lock;
public:
  my_lock_t() { pthread_mutex_init(& this->_lock, NULL); }
  ~my_lock_t() { pthread_mutex_destroy (& this->_lock); }
  void lock() { pthread_mutex_lock (& this->_lock); }
  void unlock() { pthread_mutex_unlock (& this->_lock); }
private:
  my_lock_t(const my_lock_t&); // make uncopyable
  my_lock_t& operator=(my_lock_t const&); // make unassignable
};


// RAII style mutex holder that matches { } block lifetime
struct locker
{
public:
  locker(my_lock_t *_m): m(_m) { m->lock(); }
  ~locker() { m->unlock(); }
private:
  my_lock_t* m;
};


////////////////////////////////////////////////////////////////////////


// Print a standard timestamp.
static ostream&
timestamp (ostream &o)
{
  time_t now;
  time (&now);
  char *now2 = ctime (&now);
  if (now2) {
    now2[19] = '\0';                // overwrite \n
  }

  return o << "[" << (now2 ? now2 : "") << "] "
           << "(" << getpid ()
#ifdef __linux__
           << "/" << syscall(SYS_gettid)
#else
           << "/" << pthread_self()
#endif
           << "): ";
  // XXX: tid() too
}


// A little class that impersonates an ostream to the extent that it can
// take << streaming operations.  It batches up the bits into an internal
// stringstream until it is destroyed; then flushes to the original ostream.
// It adds a timestamp
class obatched
{
private:
  ostream& o;
  stringstream stro;
  static my_lock_t lock;
public:
  obatched(ostream& oo, bool timestamp_p = true): o(oo)
  {
    if (timestamp_p)
      timestamp(stro);
  }
  ~obatched()
  {
    locker do_not_cross_the_streams(& obatched::lock);
    o << stro.str();
    o.flush();
  }
  operator ostream& () { return stro; }
  template <typename T> ostream& operator << (const T& t) { stro << t; return stro; }
};
my_lock_t obatched::lock; // just the one, since cout/cerr iostreams are not thread-safe


void reportable_exception::report(ostream& o) const {
  obatched(o) << message << endl;
}


////////////////////////////////////////////////////////////////////////


// RAII style sqlite prepared-statement holder that matches { } block lifetime

struct sqlite_ps
{
private:
  sqlite3_stmt *pp;
  
  sqlite_ps(const sqlite_ps&); // make uncopyable
  sqlite_ps& operator=(const sqlite_ps &); // make unassignable

public:
  sqlite_ps (sqlite3* db, const string& sql) {
    if (verbose > 4)
      obatched(clog) << "prep " << sql << endl;
    int rc = sqlite3_prepare_v2 (db, sql.c_str(), -1 /* to \0 */, & this->pp, NULL);
    if (rc != SQLITE_OK)
      throw sqlite_exception(rc, "prepare " + sql);
  }
  ~sqlite_ps () { sqlite3_finalize (this->pp); }
  operator sqlite3_stmt* () { return this->pp; }
};


////////////////////////////////////////////////////////////////////////

// RAII style templated autocloser

template <class Payload, class Ignore>
struct defer_dtor
{
public:
  typedef Ignore (*dtor_fn) (Payload);
  
private:
  Payload p;
  dtor_fn fn;

public:
  defer_dtor(Payload _p, dtor_fn _fn): p(_p), fn(_fn) {}
  ~defer_dtor() { (void) (*fn)(p); }

private:
  defer_dtor(const defer_dtor<Payload,Ignore>&); // make uncopyable
  defer_dtor& operator=(const defer_dtor<Payload,Ignore> &); // make unassignable
};



////////////////////////////////////////////////////////////////////////





static string
conninfo (struct MHD_Connection * conn)
{
  char hostname[128];
  char servname[128];
  int sts = -1;

  if (conn == 0)
    return "internal";

  /* Look up client address data. */
  const union MHD_ConnectionInfo *u = MHD_get_connection_info (conn,
                                                               MHD_CONNECTION_INFO_CLIENT_ADDRESS);
  struct sockaddr *so = u ? u->client_addr : 0;

  if (so && so->sa_family == AF_INET) {
    sts = getnameinfo (so, sizeof (struct sockaddr_in), hostname, sizeof (hostname), servname,
                       sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
  } else if (so && so->sa_family == AF_INET6) {
    sts = getnameinfo (so, sizeof (struct sockaddr_in6), hostname, sizeof (hostname),
                       servname, sizeof (servname), NI_NUMERICHOST | NI_NUMERICSERV);
  }
  if (sts != 0) {
    hostname[0] = servname[0] = '\0';
  }

  return string(hostname) + string(":") + string(servname);
}



////////////////////////////////////////////////////////////////////////

static void
add_mhd_last_modified (struct MHD_Response *resp, time_t mtime)
{
  struct tm *now = gmtime (&mtime);
  if (now != NULL)
    {
      char datebuf[80];
      size_t rc = strftime (datebuf, sizeof (datebuf), "%a, %d %b %Y %T GMT", now);
      if (rc > 0 && rc < sizeof (datebuf))
        (void) MHD_add_response_header (resp, "Last-Modified", datebuf);
    }
  
  (void) MHD_add_response_header (resp, "Cache-Control", "public");
}



static struct MHD_Response*
handle_buildid_f_match (int64_t b_mtime,
                        const string& b_source0)
{
  int fd = open(b_source0.c_str(), O_RDONLY);
  if (fd < 0)
    {
      if (verbose > 2)
        obatched(clog) << "cannot open " << b_source0 << endl;
      // XXX: delete the buildid record?
      // NB: it is safe to delete while a select loop is under way
      return 0;
    }
  
  // NB: use manual close(2) in error case instead of defer_dtor, because
  // in the normal case, we want to hand the fd over to libmicrohttpd for
  // file transfer.
  
  struct stat s;
  int rc = fstat(fd, &s);
  if (rc < 0)
    {
      if (verbose > 2)
        clog << "cannot fstat " << b_source0 << endl;
      close(fd);
      // XXX: delete the buildid record?
      // NB: it is safe to delete while a select loop is under way
      return 0;
    }

  if ((int64_t) s.st_mtime != b_mtime)
    {
      if (verbose > 2)
        obatched(clog) << "mtime mismatch for " << b_source0 << endl;
      close(fd);
      return 0;
    }
  
  struct MHD_Response* r = MHD_create_response_from_fd ((uint64_t) s.st_size, fd);
  if (r == 0)
    {
      if (verbose > 2)
        clog << "cannot create fd-response for " << b_source0 << endl;
      close(fd);
    }
  else
    {
      add_mhd_last_modified (r, s.st_mtime);
      if (verbose)
        obatched(clog) << "serving file " << b_source0 << endl;
      /* libmicrohttpd will close it. */
    }

  return r;
}



static struct MHD_Response*
handle_buildid_r_match (int64_t b_mtime,
                        const string& b_source0,
                        const string& b_source1)
{
  string popen_cmd = string("/usr/bin/rpm2cpio " + /* XXX sh-meta-escape */ b_source0);
  FILE* fp = popen (popen_cmd.c_str(), "r"); // "e" O_CLOEXEC?
  if (fp == NULL)
    throw libc_exception (errno, string("popen ") + popen_cmd);
  defer_dtor<FILE*,int> fp_closer (fp, pclose);

  struct archive *a;
  a = archive_read_new();
  if (a == NULL)
    throw archive_exception("cannot create archive reader");
  defer_dtor<struct archive*,int> archive_closer (a, archive_read_free);

  int rc = archive_read_support_format_cpio(a);
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot select cpio format");
  rc = archive_read_support_filter_all(a); // XXX: or _none()?  are these cpio's compressed at this point?
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot select all filters");
  
  rc = archive_read_open_FILE (a, fp);
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot open archive from rpm2cpio pipe");

  while(1) // parse cpio archive entries
    {
      struct archive_entry *e;
      rc = archive_read_next_header (a, &e);
      if (rc != ARCHIVE_OK)
        break;

      if (! S_ISREG(archive_entry_mode (e))) // skip non-files completely
        continue;
              
      string fn = archive_entry_pathname (e);
      if (fn != b_source1)
        continue;

      // extract this file to a temporary file
      char tmppath[PATH_MAX] = "/tmp/dbgserver.XXXXXX"; // XXX: $TMP_DIR etc.
      int fd = mkstemp (tmppath);
      if (fd < 0)
        throw libc_exception (errno, "cannot create temporary file");
      unlink (tmppath); // unlink now so OS will release the file as soon as we close the fd
  
      rc = archive_read_data_into_fd (a, fd);
      if (rc != ARCHIVE_OK)
        {
          close (fd);
          throw archive_exception(a, "cannot extract file");
        }

      struct MHD_Response* r = MHD_create_response_from_fd (archive_entry_size(e), fd);
      if (r == 0)
        {
          if (verbose > 2)
            clog << "cannot create fd-response for " << b_source0 << endl;
          close(fd);
        }
      else
        {
          add_mhd_last_modified (r, archive_entry_mtime(e));
          if (verbose)
            obatched(clog) << "serving rpm " << b_source0 << " file " << b_source1 << endl;
          /* libmicrohttpd will close it. */
          return r;
        }
    }

  // XXX: rpm/file not found: drop this R entry?
  return 0;
}


static struct MHD_Response*
handle_buildid_match (int64_t b_mtime,
                      const string& b_stype,
                      const string& b_source0,
                      const string& b_source1)
{
  if (b_stype == "F")
    return handle_buildid_f_match(b_mtime, b_source0);
  else if (b_stype == "R")
    return handle_buildid_r_match(b_mtime, b_source0, b_source1);
  else
    return 0;
}



static struct MHD_Response* handle_buildid (struct MHD_Connection *connection,
                const string& buildid /* unsafe */,
                const string& artifacttype /* unsafe */,
                const string& suffix /* unsafe */)
{
  // validate artifacttype
  string atype_code;
  if (artifacttype == "debuginfo") atype_code = "D";
  else if (artifacttype == "executable") atype_code = "E";
  else if (artifacttype == "source") atype_code = "S";
  else throw reportable_exception("invalid artifacttype");

  if (atype_code == "S" && suffix == "")
     throw reportable_exception("invalid source suffix");
  
  // validate buildid
  if ((buildid.size() < 2) || // not empty
      (buildid.size() % 2) || // even number
      (buildid.find_first_not_of("0123456789abcdef") != string::npos)) // pure tasty lowercase hex
    throw reportable_exception("invalid buildid");

  if (verbose)
    obatched(clog) << "searching for buildid=" << buildid << " artifacttype=" << artifacttype
         << " suffix=" << suffix << endl;
  
  sqlite_ps pp (db,
                (atype_code == "S")
                ? ("select mtime, sourcetype, source0, source1 " // NB: 4 columns
                   "from " BUILDIDS " where buildid = ? and artifacttype = ? and artifactsrc = ?"
                   " order by mtime desc;")
                : ("select mtime, sourcetype, source0, source1 " // NB: 4 columns
                   "from " BUILDIDS " where buildid = ? and artifacttype = ? and artifactsrc is null"
                   " order by mtime desc;"));

  int rc = sqlite3_bind_text (pp, 1, buildid.c_str(), -1 /* to \0 */, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
    throw sqlite_exception(rc, "bind 1");
  rc = sqlite3_bind_text (pp, 2, atype_code.c_str(), -1 /* to \0 */, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      throw sqlite_exception(rc, "bind 2");
  if (atype_code == "S") // source
    rc = sqlite3_bind_text (pp, 3, suffix.c_str(), -1 /* to \0 */, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      throw sqlite_exception(rc, "bind 3");
  
  // consume all the rows
  while (1)
    {
      rc = sqlite3_step (pp);
      if (rc == SQLITE_DONE) break;
      if (rc != SQLITE_ROW)
        throw sqlite_exception(rc, "step");
      
      int64_t b_mtime = sqlite3_column_int64 (pp, 0);
      string b_stype = string((const char*) sqlite3_column_text (pp, 1) ?: ""); /* by DDL may not be NULL */
      string b_source0 = string((const char*) sqlite3_column_text (pp, 2) ?: ""); /* may be NULL */
      string b_source1 = string((const char*) sqlite3_column_text (pp, 3) ?: ""); /* may be NULL */

      if (verbose > 1)
        obatched(clog) << "found mtime=" << b_mtime << " stype=" << b_stype
             << " source0=" << b_source0 << " source1=" << b_source1 << endl;

      // Try accessing the located match.
      // XXX: validate the mtime against that in the column
      // XXX: in case of multiple matches, attempt them in parallel?
      auto r = handle_buildid_match (b_mtime, b_stype, b_source0, b_source1);
      if (r)
        return r;
    }

  // We couldn't find it in the database.  Last ditch effort
  // is to defer to other debuginfo servers.
  int fd = -1;
  if (artifacttype == "debuginfo")
    fd = dbgserver_find_debuginfo ((const unsigned char*) buildid.c_str(), 0,
                                   NULL);
  else if (artifacttype == "executable")
    fd = dbgserver_find_executable ((const unsigned char*) buildid.c_str(), 0,
                                    NULL);
  else if (artifacttype == "source")
    fd = dbgserver_find_source ((const unsigned char*) buildid.c_str(), 0,
                                suffix.c_str(), NULL);
  // XXX: report bad fd
  if (fd >= 0)
    {
      struct stat s;
      rc = fstat (fd, &s);
      if (rc == 0)
        {
          auto r = MHD_create_response_from_fd ((uint64_t) s.st_size, fd);
          if (r)
            {
              add_mhd_last_modified (r, s.st_mtime);
              if (verbose)
                obatched(clog) << "serving file from upstream dbgserver/cache" << endl;
              return r; // NB: don't close fd; libmicrohttpd will
            }
        }
      close (fd);
    }
  
  throw reportable_exception(MHD_HTTP_NOT_FOUND, "not found");
}


////////////////////////////////////////////////////////////////////////


static struct MHD_Response*
handle_metrics (struct MHD_Connection *connection)
{
  throw reportable_exception("not yet implemented 2");
}


////////////////////////////////////////////////////////////////////////


/* libmicrohttpd callback */
static int
handler_cb (void *cls  __attribute__ ((unused)),
            struct MHD_Connection *connection,
            const char *url,
            const char *method,
            const char *version  __attribute__ ((unused)),
            const char *upload_data  __attribute__ ((unused)),
            size_t * upload_data_size __attribute__ ((unused)),
            void ** con_cls __attribute__ ((unused)))
{
  struct MHD_Response *r = NULL;
  string url_copy = url;
  
  if (verbose)
    obatched(clog) << conninfo(connection) << " " << method << " " << url << endl;

  try
    {
      if (string(method) != "GET")
        throw reportable_exception(400, _("we support GET only"));

      /* Start decoding the URL. */
      size_t slash1 = url_copy.find('/', 1);
      string url1 = url_copy.substr(0, slash1); // ok even if slash1 not found
      
      if (slash1 != string::npos && url1 == "/buildid")
        {
          size_t slash2 = url_copy.find('/', slash1+1);
          if (slash2 == string::npos)
            throw reportable_exception(_("/buildid/ webapi error, need buildid"));
          
          string buildid = url_copy.substr(slash1+1, slash2-slash1-1);

          size_t slash3 = url_copy.find('/', slash2+1);
          string artifacttype, suffix;
          if (slash3 == string::npos)
            {
              artifacttype = url_copy.substr(slash2+1);
              suffix = "";
            }
          else
            {
              artifacttype = url_copy.substr(slash2+1, slash3-slash2-1);
              suffix = url_copy.substr(slash3); // include the slash in the suffix
            }
          
          r = handle_buildid(connection, buildid, artifacttype, suffix);
        }
      else if (url1 == "/metrics")
        r = handle_metrics(connection);
      else
        throw reportable_exception(_("webapi error, unrecognized /operation"));
      
      if (r == 0)
        throw reportable_exception(_("internal error, missing response"));
      
      int rc = MHD_queue_response (connection, MHD_HTTP_OK, r);
      MHD_destroy_response (r);
      return rc;
    }
  catch (const reportable_exception& e)
    {
      e.report(clog);
      return e.mhd_send_response (connection);
    }
}


////////////////////////////////////////////////////////////////////////


// borrowed from src/nm.c get_local_names()

static void
dwarf_extract_source_paths (Elf *elf, GElf_Ehdr* ehdr, Elf_Scn* scn, GElf_Shdr* shdr, set<string>& debug_sourcefiles)
{
  Dwarf* dbg = dwarf_begin_elf (elf, DWARF_C_READ, NULL);
  if (dbg == NULL)
    return;

  Dwarf_Off offset = 0;
  Dwarf_Off old_offset;
  size_t hsize;

  while (dwarf_nextcu (dbg, old_offset = offset, &offset, &hsize, NULL, NULL, NULL) == 0)
    {
      Dwarf_Die cudie_mem;
      Dwarf_Die *cudie = dwarf_offdie (dbg, old_offset + hsize, &cudie_mem);

      if (cudie == NULL)
        continue;
      if (dwarf_tag (cudie) != DW_TAG_compile_unit)
        continue;

      const char *cuname = dwarf_diename(cudie) ?: "unknown";

      Dwarf_Files *files;
      size_t nfiles;
      if (dwarf_getsrcfiles (cudie, &files, &nfiles) != 0)
        continue;

      // extract DW_AT_comp_dir to resolve relative file names
      const char *comp_dir = "";
      const char *const *dirs;
      size_t ndirs;
      if (dwarf_getsrcdirs (files, &dirs, &ndirs) == 0 &&
          dirs[0] != NULL)
        comp_dir = dirs[0];

      if (verbose > 3)
        obatched(clog) << "Searching for sources for cu=" << cuname << " comp_dir=" << comp_dir
                       << " #files=" << nfiles << " #dirs=" << ndirs << endl;
      
      for (size_t f = 1; f < nfiles; f++)
        {
          const char *hat = dwarf_filesrc (files, f, NULL, NULL);
          if (hat == NULL)
            continue;
            
          string waldo;
          if (hat[0] == '/') // absolute
            waldo = (string (hat));
          else // comp_dir relative
            waldo = (string (comp_dir) + string("/") + string (hat));
          
          // NB: this is the 'waldo' that a dbginfo client will have
          // to supply for us to give them the file The comp_dir
          // prefixing is a definite complication.  Otherwise we'd
          // have to return a setof comp_dirs (one per CU!) with
          // corresponding filesrc[] names, instead of one absolute
          // resoved set.  Maybe we'll have to do that anyway.  XXX

          if (verbose > 4)
            obatched(clog) << waldo
                           << (debug_sourcefiles.find(waldo)==debug_sourcefiles.end() ? " new" : " dup") <<  endl;
          
          debug_sourcefiles.insert (waldo);
        }
    }

  dwarf_end(dbg);
}



static void
elf_classify (int fd, bool &executable_p, bool &debuginfo_p, string &buildid, set<string>& debug_sourcefiles)
{
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
  if (elf == NULL)
    return;
  
  try // catch our types of errors and clean up the Elf* object
    {
      if (elf_kind (elf) != ELF_K_ELF)
        {
          elf_end (elf);
          return;
        }

      GElf_Ehdr ehdr_storage;
      GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_storage);
      if (ehdr == NULL)
        {
          elf_end (elf);
          return;
        }
      auto elf_type = ehdr->e_type;
  
      const void *build_id; // elfutils-owned memory
      ssize_t sz = dwelf_elf_gnu_build_id (elf, & build_id);
      if (sz <= 0)
        {
          // It's not a diagnostic-worthy error for an elf file to lack build-id.
          // It might just be very old.
          elf_end (elf);
          return;
        }
  
      // build_id is a raw byte array; convert to hexadecimal *lowercase*
      unsigned char* build_id_bytes = (unsigned char*) build_id;
      for (ssize_t idx=0; idx<sz; idx++)
        {
          buildid += "0123456789abcdef"[build_id_bytes[idx] >> 4];
          buildid += "0123456789abcdef"[build_id_bytes[idx] & 0xf];
        }

      // now decide whether it's an executable - namely, any allocatable section has
      // PROGBITS;
      if (elf_type == ET_EXEC || elf_type == ET_DYN)
        {
          size_t shnum;
          int rc = elf_getshdrnum (elf, &shnum);
          if (rc < 0)
            throw elfutils_exception(rc, "getshdrnum");

          executable_p = false;
          for (size_t sc = 0; sc < shnum; sc++)
            {
              Elf_Scn *scn = elf_getscn (elf, sc);
              if (scn == NULL)
                continue;

              GElf_Shdr shdr_mem;
              GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
              if (shdr == NULL)
                continue;

              // allocated (loadable / vm-addr-assigned) section with available content?
              if ((shdr->sh_type == SHT_PROGBITS) && (shdr->sh_flags & SHF_ALLOC))
                {
                  if (verbose > 5)
                    obatched(clog) << "executable due to SHF_ALLOC SHT_PROGBITS sc=" << sc << endl;
                  executable_p = true;
                  break; // no need to keep looking for others
                }
            } // iterate over sections
        } // executable_p classification

      // now decide whether it's a debuginfo - namely, if it has any .debug* or .zdebug* sections
      // logic mostly stolen from fweimer@redhat.com's elfclassify drafts
      size_t shstrndx;
      int rc = elf_getshdrstrndx (elf, &shstrndx);
      if (rc < 0)
        throw elfutils_exception(rc, "getshdrstrndx");
    
      Elf_Scn *scn = NULL;
      while (true)
        {
          scn = elf_nextscn (elf, scn);
          if (scn == NULL)
            break;
          GElf_Shdr shdr_storage;
          GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_storage);
          if (shdr == NULL)
            break;
          const char *section_name = elf_strptr (elf, shstrndx, shdr->sh_name);
          if (section_name == NULL)
            break;
          if (strncmp(section_name, ".debug_line", 11) == 0 ||
              strncmp(section_name, ".zdebug_line", 12) == 0)
            {
              debuginfo_p = true;
              dwarf_extract_source_paths (elf, ehdr, scn, shdr, debug_sourcefiles);
              break; // expecting only one .*debug_line, so no need to look for others
            }
          else if (strncmp(section_name, ".debug_", 7) == 0 ||
                   strncmp(section_name, ".zdebug_", 8) == 0)
            {
              debuginfo_p = true;
              // NB: don't break; need to parse .debug_line for sources
            }
        }
    }
  catch (const reportable_exception& e)
    {
      e.report(clog);
    }
  elf_end (elf);
}



static void
scan_source_file_path (const string& dir)
{
  sqlite_ps ps_upsert_buildids (db, "insert or ignore into " BUILDIDS "_buildids VALUES (NULL, ?);");
  sqlite_ps ps_upsert_files (db, "insert or ignore into " BUILDIDS "_files VALUES (NULL, ?);");
  sqlite_ps ps_upsert (db,
                       "insert or replace into " BUILDIDS "_norm "
                       "(buildid, artifacttype, artifactsrc, mtime, sourcetype, source0) "
                       "values ((select id from " BUILDIDS "_buildids where hex = ?),"
                       "        ?,"
                       "        (select id from " BUILDIDS "_files where name = ?), ?, 'F',"
                       "        (select id from " BUILDIDS "_files where name = ?));");
  sqlite_ps ps_query (db,
                      "select 1 from " BUILDIDS "_norm where sourcetype = 'F' and source0 = (select id from " BUILDIDS "_files where name = ?) and mtime = ? limit 1;");
  sqlite_ps ps_cleanup (db, "delete from " BUILDIDS "_norm where mtime < ? and sourcetype = 'F' and source0 = (select id from " BUILDIDS "_files where name = ?);");
  // find the source BOLOs
  sqlite_ps ps_bolo_insert (db, "insert or ignore into " BUILDIDS "_bolo values (?, ?, 'F', ?);");
  sqlite_ps ps_bolo_find (db, "select buildid,srcname from " BUILDIDS "_bolo where sourcetype = 'F' and dirname = ?;");
  sqlite_ps ps_bolo_nuke (db, "delete from " BUILDIDS "_bolo where sourcetype = 'F' and dirname = ?;");
  
  char * const dirs[] = { (char*) dir.c_str(), NULL };

  struct timeval tv_start, tv_end;
  unsigned fts_scanned=0, fts_cached=0, fts_debuginfo=0, fts_executable=0, fts_sourcefiles=0;
  gettimeofday (&tv_start, NULL);
  
  FTS *fts = fts_open (dirs,
                       FTS_PHYSICAL /* don't follow symlinks */
                       | FTS_XDEV /* don't cross devices/mountpoints */
                       | FTS_NOCHDIR /* multithreaded */,
                       NULL);
  if (fts == NULL)
    {
      obatched(cerr) << "cannot fts_open " << dir << endl;
      return;
    }

  vector<string> directory_stack; // to allow knowledge of fts $DIR
  FTSENT *f;
  while ((f = fts_read (fts)) != NULL)
    {
      fts_scanned ++;
      if (interrupted)
        break;

      if (verbose > 3)
        obatched(clog) << "fts traversing " << f->fts_path << endl;

      try
        {
          /* Found a file.  Convert it to an absolute path, so
             the buildid database does not have relative path
             names that are unresolvable from a subsequent run
             in a different cwd. */
          char *rp = realpath(f->fts_path, NULL);
          if (rp == NULL)
            throw libc_exception(errno, "fts realpath " + string(f->fts_path));
          string rps = string(rp);
          free (rp);
          
          int rc = 0;
          switch (f->fts_info)
            {
            case FTS_D:
              directory_stack.push_back (rps);
              break;

            case FTS_DP:
              if (directory_stack.size() > 0) // in case FTS_D and FTS_DP don't quite line up
                directory_stack.pop_back ();
              // Finished traversing this directory (hierarchy).  Check for any source files that can be
              // reached from here.

              sqlite3_reset (ps_bolo_find);
              rc = sqlite3_bind_text (ps_bolo_find, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 bolo-find bind1");

              while (1)
                {
                  rc = sqlite3_step (ps_bolo_find);
                  if (rc == SQLITE_DONE)
                    break;
                  else if (rc == SQLITE_ROW) // i.e., a result, as opposed to DONE (no results)
                    {
                      string buildid = string((const char*) sqlite3_column_text (ps_bolo_find, 0) ?: "NULL"); // NULL can't happen
                      string dwarfsrc = string((const char*) sqlite3_column_text (ps_bolo_find, 1) ?: "NULL"); // NULL can't happen
                      
                      string srcpath;
                      if (dwarfsrc.size() > 0 && dwarfsrc[0] == '/') // src file name is absolute, use as is
                        srcpath = dwarfsrc;
                      else
                        srcpath = rps + string("/") + dwarfsrc; // XXX: should not happen; elf_classify only gives back /absolute files
                      
                      char *srp = realpath(srcpath.c_str(), NULL);
                      if (srp == NULL)
                        continue; // unresolvable files are not a serious problem
                        // throw libc_exception(errno, "fts realpath " + srcpath);
                      string srps = string(srp);
                      free (srp);

                      struct stat sfs;
                      rc = stat(srps.c_str(), &sfs);
                      if (rc == 0)
                        {
                          if (verbose > 2)
                            obatched(clog) << "recorded buildid=" << buildid << " file=" << srps
                                           << " mtime=" << sfs.st_mtime
                                           << " as source " << dwarfsrc << endl;

                          // register this file name in the interning table
                          sqlite3_reset (ps_upsert_files);
                          rc = sqlite3_bind_text (ps_upsert_files, 1, srps.c_str(), -1, SQLITE_TRANSIENT);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo-file bind");           
                          rc = sqlite3_step (ps_upsert_files);
                          if (rc != SQLITE_OK && rc != SQLITE_DONE)
                            throw sqlite_exception(rc, "sqlite3 bolo-file execute");           

                          // register the dwarfsrc name in the interning table too
                          sqlite3_reset (ps_upsert_files);
                          rc = sqlite3_bind_text (ps_upsert_files, 1, dwarfsrc.c_str(), -1, SQLITE_TRANSIENT);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo-file bind");           
                          rc = sqlite3_step (ps_upsert_files);
                          if (rc != SQLITE_OK && rc != SQLITE_DONE)
                            throw sqlite_exception(rc, "sqlite3 bolo-file execute");           
                          
                          sqlite3_reset (ps_upsert);
                          rc = sqlite3_bind_text (ps_upsert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert bind1");
                          rc = sqlite3_bind_text (ps_upsert, 2, "S", -1, SQLITE_STATIC);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert bind2");
                          rc = sqlite3_bind_text (ps_upsert, 3, dwarfsrc.c_str(), -1, SQLITE_TRANSIENT);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert bind2");
                          rc = sqlite3_bind_int64 (ps_upsert, 4, (int64_t) sfs.st_mtime);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert bind3");           
                          rc = sqlite3_bind_text (ps_upsert, 5, srps.c_str(), -1, SQLITE_TRANSIENT);
                          if (rc != SQLITE_OK)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert bind3");           

                          rc = sqlite3_step (ps_upsert);
                          if (rc != SQLITE_OK && rc != SQLITE_DONE)
                            throw sqlite_exception(rc, "sqlite3 bolo upsert execute");
                        }
                    }
                  else
                    throw sqlite_exception(rc, "sqlite3 bolo-find step");
                } // loop over bolo records

              if (verbose > 2)
                obatched(clog) << "nuking bolo for directory=" << rps << endl;
              
              // ditch matching bolo records so we don't repeat search
              sqlite3_reset (ps_bolo_nuke);
              rc = sqlite3_bind_text (ps_bolo_nuke, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 bolo-nuke bind1");
              rc = sqlite3_step (ps_bolo_nuke);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 bolo-nuke execute");           
                          
              break;

            case FTS_F:
              {
                /* See if we know of it already. */
                sqlite3_reset (ps_query); // to allow rebinding / reexecution
                int rc = sqlite3_bind_text (ps_query, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 file query bind1");
                rc = sqlite3_bind_int64 (ps_query, 2, (int64_t) f->fts_statp->st_mtime);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 file query bind2");
                rc = sqlite3_step (ps_query);
                if (rc == SQLITE_ROW) // i.e., a result, as opposed to DONE (no results)
                  // no need to recheck a file/version we already know
                  // specifically, no need to elf-begin a file we already determined is non-elf
                  // (so is stored with buildid=NULL)
                  {
                    fts_cached ++;
                    continue;
                  }

                bool executable_p = false, debuginfo_p = false; // E and/or D
                string buildid;
                set<string> sourcefiles;
                
                int fd = open (rps.c_str(), O_RDONLY);
                try
                  {
                    if (fd >= 0)
                      elf_classify (fd, executable_p, debuginfo_p, buildid, sourcefiles);
                    else
                      throw libc_exception(errno, string("open ") + rps);
                  }
                
                // NB: we catch exceptions from elf_classify here too, so that we can
                // cache the corrupt-elf case (!executable_p && !debuginfo_p) just below,
                // just as if we had an EPERM error from open(2).
                    
                catch (const reportable_exception& e)
                  {
                    e.report(clog);
                  }
                    
                if (fd >= 0)
                  close (fd);

                // register this file name in the interning table
                sqlite3_reset (ps_upsert_files);
                rc = sqlite3_bind_text (ps_upsert_files, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert-file bind");           
                rc = sqlite3_step (ps_upsert_files);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 upsert-file execute");           
                
                if (buildid == "")
                  {
                    sqlite3_reset (ps_upsert); // to allow rebinding / reexecution
                    rc = sqlite3_bind_null (ps_upsert, 1);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert bind1");
                    // no point storing an elf file without buildid
                    executable_p = false;
                    debuginfo_p = false;
                  }
                else
                  {
                    // register this build-id in the interning table
                    sqlite3_reset (ps_upsert_buildids);
                    rc = sqlite3_bind_text (ps_upsert_buildids, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-buildid bind");           
                    rc = sqlite3_step (ps_upsert_buildids);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-buildid execute");           
                    
                    sqlite3_reset (ps_upsert); // to allow rebinding / reexecution
                    rc = sqlite3_bind_text (ps_upsert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert bind1");
                  }
                
                // artifacttype column 2 set later
                rc = sqlite3_bind_null (ps_upsert, 3); // no artifactsrc for D/E
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind3");
                rc = sqlite3_bind_int64 (ps_upsert, 4, (int64_t) f->fts_statp->st_mtime);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind4");           
                rc = sqlite3_bind_text (ps_upsert, 5, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind5");           
                
                if (executable_p)
                  {
                    fts_executable ++;
                    rc = sqlite3_bind_text (ps_upsert, 2, "E", -1, SQLITE_STATIC);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-E bind2");           
                    rc = sqlite3_step (ps_upsert);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-E execute");           
                  }
          
                if (debuginfo_p)
                  {
                    fts_debuginfo ++;
                    sqlite3_reset (ps_upsert); // to allow rebinding / reexecution
                    rc = sqlite3_bind_text (ps_upsert, 2, "D", -1, SQLITE_STATIC);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-D bind2");           
                    rc = sqlite3_step (ps_upsert);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-D execute");
                  }

                if (sourcefiles.size() && buildid != "")
                  {
                    fts_sourcefiles += sourcefiles.size();
                    string sourcedir;
                    if (directory_stack.size() == 0) // in case -F /path/to/file or FTS_D didn't line up with FTS_DP
                      sourcedir = ".";
                    else
                      sourcedir = directory_stack.back ();
                    
                    for (auto&& sf : sourcefiles)
                      {
                        sqlite3_reset (ps_bolo_insert);
                        rc = sqlite3_bind_text (ps_bolo_insert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                        if (rc != SQLITE_OK)
                          throw sqlite_exception(rc, "sqlite3 upsert-bolo bind1");
                        rc = sqlite3_bind_text (ps_bolo_insert, 2, sf.c_str(), -1, SQLITE_TRANSIENT);
                        if (rc != SQLITE_OK)
                          throw sqlite_exception(rc, "sqlite3 upsert-bolo bind2");
                        rc = sqlite3_bind_text (ps_bolo_insert, 3, sourcedir.c_str(), -1, SQLITE_TRANSIENT);
                        if (rc != SQLITE_OK)
                          throw sqlite_exception(rc, "sqlite3 upsert-bolo bind3");
                        
                        rc = sqlite3_step (ps_bolo_insert);
                        if (rc != SQLITE_OK && rc != SQLITE_DONE)
                          throw sqlite_exception(rc, "sqlite3 upsert-bolo execute");
                      }


                    
                  }
                
                if (! (executable_p || debuginfo_p))  // negative hit
                  {
                    rc = sqlite3_bind_null (ps_upsert, 2);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL bind2");
                    rc = sqlite3_step (ps_upsert);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL execute");
                  }

                // clean up any older entries for this file, in case it was replaced/recompiled to new buildid
                sqlite3_reset (ps_cleanup);
                rc = sqlite3_bind_int64 (ps_cleanup, 1, (int64_t) f->fts_statp->st_mtime);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 cleanup bind1");           
                rc = sqlite3_bind_text (ps_cleanup, 2, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 cleanup bind2");           
                rc = sqlite3_step (ps_cleanup);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 cleanup exec");           
                
                if (verbose > 2)
                  obatched(clog) << "recorded buildid=" << buildid << " file=" << rps
                                 << " mtime=" << f->fts_statp->st_mtime << " as "
                                 << (executable_p ? "executable" : "not executable") << " and "
                                 << (debuginfo_p ? "debuginfo" : "not debuginfo") << endl;


                
                // XXX: delete earlier records for the same file (mtime < this_mtime)
              }
              break;

            case FTS_ERR:
            case FTS_NS:
              throw libc_exception(f->fts_errno, string("fts traversal ") + string(f->fts_path));

            default:
            case FTS_SL: /* NB: don't enter symbolic links into the database */
              break;
            }
        }
      catch (const reportable_exception& e)
        {
          e.report(clog);
        }
    }
  fts_close (fts);

  gettimeofday (&tv_end, NULL);
  double deltas = (tv_end.tv_sec - tv_start.tv_sec) + (tv_end.tv_usec - tv_start.tv_usec)*0.000001;
  
  if (verbose > 1)
    obatched(clog) << "fts traversed " << dir << " in " << deltas << "s, scanned=" << fts_scanned
                   << ", cached=" << fts_cached << ", debuginfo=" << fts_debuginfo
                   << ", executable=" << fts_executable << ", source=" << fts_sourcefiles << endl;
}


static void*
thread_main_scan_source_file_path (void* arg)
{
  string dir = string((const char*) arg);
  if (verbose > 2)
    obatched(clog) << "file-path scanning " << dir << endl;

  unsigned rescan_timer = 0;
  while (! interrupted)
    {
      try
        {
          if (rescan_timer == 0)
            scan_source_file_path (dir);
        }
      catch (const sqlite_exception& e)
        {
          obatched(cerr) << e.message << endl;
        }
      sleep (1);
      rescan_timer ++;
      if (rescan_s)
        rescan_timer %= rescan_s;
    }
  
  return 0;
}


////////////////////////////////////////////////////////////////////////



// Analyze given *.rpm file of given age; record buildids / exec/debuginfo-ness of its
// constituent files with given upsert statement.
static void
rpm_classify (const string& rps, sqlite_ps& ps_upsert_buildids, sqlite_ps& ps_upsert_files,
              sqlite_ps& ps_upsert, sqlite_ps& ps_bolo_upsert, sqlite_ps& ps_rfolo_upsert,
              time_t mtime, const string& dirname,
              unsigned& fts_executable, unsigned& fts_debuginfo, unsigned& fts_sourcefiles)
{
  string popen_cmd = string("/usr/bin/rpm2cpio " + /* XXX sh-meta-escape */ rps);
  FILE* fp = popen (popen_cmd.c_str(), "r"); // "e" O_CLOEXEC?
  if (fp == NULL)
    throw libc_exception (errno, string("popen ") + popen_cmd);
  defer_dtor<FILE*,int> fp_closer (fp, pclose);

  struct archive *a;
  a = archive_read_new();
  if (a == NULL)
    throw archive_exception("cannot create archive reader");
  defer_dtor<struct archive*,int> archive_closer (a, archive_read_free);

  int rc = archive_read_support_format_cpio(a);
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot select cpio format");
  rc = archive_read_support_filter_all(a); // XXX: or _none()?  are these cpio's compressed at this point?
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot select all filters");
  
  rc = archive_read_open_FILE (a, fp);
  if (rc != ARCHIVE_OK)
    throw archive_exception(a, "cannot open archive from rpm2cpio pipe");

  if (verbose > 3)
    obatched(clog) << "rpm2cpio|libarchive scanning " << rps << endl;
  
  while(1) // parse cpio archive entries
    {
      try
        {
          struct archive_entry *e;
          rc = archive_read_next_header (a, &e);
          if (rc != ARCHIVE_OK)
            break;

          if (! S_ISREG(archive_entry_mode (e))) // skip non-files completely
            continue;
              
          string fn = archive_entry_pathname (e);

          // add the rfolo record for this pathname
          sqlite3_reset (ps_rfolo_upsert);
          rc = sqlite3_bind_text (ps_rfolo_upsert, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert-rfolo bind1");
          rc = sqlite3_bind_int64 (ps_rfolo_upsert, 2, (int64_t) mtime);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert-rfolo bind2");
          rc = sqlite3_bind_text (ps_rfolo_upsert, 3, fn.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert-rfolo bind3");
          rc = sqlite3_bind_text (ps_rfolo_upsert, 4, dirname.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert-rfolo bind4");
          rc = sqlite3_step (ps_rfolo_upsert);
          if (rc != SQLITE_OK && rc != SQLITE_DONE)
            throw sqlite_exception(rc, "sqlite3 upsert-rfolo execute");
          
          if (verbose > 3)
            obatched(clog) << "rpm2cpio|libarchive checking " << fn << endl;

          // extract this file to a temporary file
          char tmppath[PATH_MAX] = "/tmp/dbgserver.XXXXXX"; // XXX: $TMP_DIR etc.
          int fd = mkstemp (tmppath);
          if (fd < 0)
            throw libc_exception (errno, "cannot create temporary file");
          unlink (tmppath); // unlink now so OS will release the file as soon as we close the fd
          defer_dtor<int,int> minifd_closer (fd, close);
  
          rc = archive_read_data_into_fd (a, fd);
          if (rc != ARCHIVE_OK)
            throw archive_exception(a, "cannot extract file");

          // finally ... time to run elf_classify on this bad boy and update the database
          bool executable_p = false, debuginfo_p = false;
          string buildid;
          set<string> sourcefiles;
          elf_classify (fd, executable_p, debuginfo_p, buildid, sourcefiles);
          // NB: might throw

          // NB: we record only executable_p || debuginfo_p case here,
          // not the 'neither' case.

          if (buildid != "") // intern file name
            {
              sqlite3_reset (ps_upsert_buildids);
              rc = sqlite3_bind_text (ps_upsert_buildids, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-bid bind1");
              rc = sqlite3_step (ps_upsert_buildids);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-bid execute");
            }

          if (executable_p || debuginfo_p) // intern file name
            {
              sqlite3_reset (ps_upsert_files);
              rc = sqlite3_bind_text (ps_upsert_files, 1, fn.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-rpm1 bind1");
              rc = sqlite3_step (ps_upsert_buildids);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-rpm1 execute");
            }

          if (sourcefiles.size() > 0) // intern all the source files
            {
              // NB: we intern each source file -twice-.  Once raw, as
              // it appears in the DWARF file list coming back from
              // elf_classify() - because it'll end up in the
              // _norm.artifactsrc column.  Plus: once with a "." at
              // the front, because that's how we'll expect it'll show
              // up in one of the -debuginfo|source rpms and therefore
              // in the _norm.source1 and _rfolo.source1 fields.  (We
              // don't want to preemptively intern ALL file names we
              // get from scanning RPMs, because most of them are not
              // going to be debuginfo-related, thus would needlessly
              // bloat the interning table.)

              for (auto&& s : sourcefiles)
                {
                  if (s.size() == 0) continue;
                  
                  sqlite3_reset (ps_upsert_files);
                  rc = sqlite3_bind_text (ps_upsert_files, 1, s.c_str(), -1, SQLITE_TRANSIENT);
                  if (rc != SQLITE_OK)
                    throw sqlite_exception(rc, "sqlite3 upsert-sf1 bind1");
                  rc = sqlite3_step (ps_upsert_files);
                  if (rc != SQLITE_OK && rc != SQLITE_DONE)
                    throw sqlite_exception(rc, "sqlite3 upsert-sf1 execute");

                  if (s[0] == '/') // the normal case
                    {
                      string sdot = string(".") + s;
                      sqlite3_reset (ps_upsert_files);
                      rc = sqlite3_bind_text (ps_upsert_files, 1, sdot.c_str(), -1, SQLITE_TRANSIENT);
                      if (rc != SQLITE_OK)
                        throw sqlite_exception(rc, "sqlite3 upsert-sf2 bind1");
                      rc = sqlite3_step (ps_upsert_files);
                      if (rc != SQLITE_OK && rc != SQLITE_DONE)
                        throw sqlite_exception(rc, "sqlite3 upsert-sf2 execute");
                    }

                  // now add the bolo record
                  if (buildid != "")
                    {
                      sqlite3_reset (ps_bolo_upsert);
                      rc = sqlite3_bind_text (ps_bolo_upsert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                      if (rc != SQLITE_OK)
                        throw sqlite_exception(rc, "sqlite3 upsert-bolo bind1");
                      rc = sqlite3_bind_text (ps_bolo_upsert, 2, s.c_str(), -1, SQLITE_TRANSIENT);
                      if (rc != SQLITE_OK)
                        throw sqlite_exception(rc, "sqlite3 upsert-bolo bind2");
                      rc = sqlite3_bind_text (ps_bolo_upsert, 3, dirname.c_str(), -1, SQLITE_TRANSIENT);
                      if (rc != SQLITE_OK)
                        throw sqlite_exception(rc, "sqlite3 upsert-bolo bind3");
                      rc = sqlite3_step (ps_bolo_upsert);
                      if (rc != SQLITE_OK && rc != SQLITE_DONE)
                        throw sqlite_exception(rc, "sqlite3 upsert-bolo execute");
                    }

                  fts_sourcefiles ++;
                }
            }

          sqlite3_reset (ps_upsert); // to allow rebinding / reexecution          
          rc = sqlite3_bind_text (ps_upsert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert bind1");
          rc = sqlite3_bind_int64 (ps_upsert, 3, (int64_t) mtime);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert bind3");           
          rc = sqlite3_bind_text (ps_upsert, 4, fn.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert bind4");           
          rc = sqlite3_bind_text (ps_upsert, 5, rps.c_str(), -1, SQLITE_TRANSIENT);
          if (rc != SQLITE_OK)
            throw sqlite_exception(rc, "sqlite3 upsert bind5");           

          if (executable_p)
            {
              fts_executable ++;

              // register this rpm-subfile name in the interning table
              sqlite3_reset (ps_upsert_files);
              rc = sqlite3_bind_text (ps_upsert_files, 1, fn.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-file bind");           
              rc = sqlite3_step (ps_upsert_files);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-file execute");           
          
              rc = sqlite3_bind_text (ps_upsert, 2, "E", -1, SQLITE_STATIC);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-E bind2");           
              rc = sqlite3_step (ps_upsert);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-E execute");           
            }
          
          if (debuginfo_p)
            {
              fts_debuginfo ++;
              
              // register this rpm-subfile name in the interning table
              sqlite3_reset (ps_upsert_files);
              rc = sqlite3_bind_text (ps_upsert_files, 1, fn.c_str(), -1, SQLITE_TRANSIENT);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-file bind");           
              rc = sqlite3_step (ps_upsert_files);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-file execute");           
          
              sqlite3_reset (ps_upsert); // to allow rebinding / reexecution
              rc = sqlite3_bind_text (ps_upsert, 2, "D", -1, SQLITE_STATIC);
              if (rc != SQLITE_OK)
                throw sqlite_exception(rc, "sqlite3 upsert-D bind2");           
              rc = sqlite3_step (ps_upsert);
              if (rc != SQLITE_OK && rc != SQLITE_DONE)
                throw sqlite_exception(rc, "sqlite3 upsert-D execute");
            }

          if ((verbose > 2) && (executable_p || debuginfo_p))
            obatched(clog) << "recorded buildid=" << buildid << " rpm=" << rps << " file=" << fn
                           << " mtime=" << mtime << " as "
                           << (executable_p ? "executable" : "not executable") << " and "
                           << (debuginfo_p ? "debuginfo" : "not debuginfo")
                           << " sourcefiles=" << sourcefiles.size() << endl;
          
        }
      catch (const reportable_exception& e)
        {
          e.report(clog);
        }
    }
}



// scan for *.rpm files
static void
scan_source_rpm_path (const string& dir)
{
  sqlite_ps ps_upsert_buildids (db, "insert or ignore into " BUILDIDS "_buildids VALUES (NULL, ?);");
  sqlite_ps ps_upsert_files (db, "insert or ignore into " BUILDIDS "_files VALUES (NULL, ?);");
  sqlite_ps ps_upsert (db,
                       "insert or replace into " BUILDIDS "_norm (buildid, artifacttype, artifactsrc, mtime,"
                       "sourcetype, source0, source1) values ("
                       "(select id from " BUILDIDS "_buildids where hex = ?), ?, NULL, ?, 'R',"
                       "(select id from " BUILDIDS "_files where name = ?),"
                       "(select id from " BUILDIDS "_files where name = ?));");
  sqlite_ps ps_query (db,
                      "select 1 from " BUILDIDS " where sourcetype = 'R' and source0 = ? and mtime = ? limit 1;");
  sqlite_ps ps_upsert_bolo_rfolo_join (db,
                                       "insert or replace into " BUILDIDS "_norm (buildid, artifacttype, artifactsrc, mtime,"
                                       "sourcetype, source0, source1) "
                                       "select b.id, 'S', fb.id, rfolo.mtime, bolo.sourcetype, f0.id, f1.id "
                                       "from " BUILDIDS "_buildids b, " BUILDIDS "_bolo bolo, " BUILDIDS "_rfolo rfolo, "
                                       BUILDIDS "_files f0, " BUILDIDS "_files f1, " BUILDIDS "_files fb "
                                       "where b.hex = bolo.buildid and "
                                       "'.'||bolo.srcname = rfolo.source1 and " // RPMs have . name prefix for cpio contents
                                       "bolo.sourcetype = 'R' and bolo.dirname = ? and rfolo.dirname = bolo.dirname and "
                                       "f0.name = rfolo.source0 and f1.name = rfolo.source1 and fb.name = bolo.srcname"
                                       /// XXXXXX add  NULL ... entries for rfolo rpms that have no bolo-sought content, so we don't have to open it again
                                       );
  sqlite_ps ps_bolo_nuke (db,
                          "delete from " BUILDIDS "_bolo where sourcetype = 'R' and dirname = ?;");
  sqlite_ps ps_bolo_upsert (db,
                             "insert or replace into " BUILDIDS "_bolo (buildid, srcname, sourcetype, dirname) values (?, ?, 'R', ?);");
  sqlite_ps ps_rfolo_nuke (db,
                            "delete from " BUILDIDS "_rfolo where dirname = ?;");
  sqlite_ps ps_rfolo_upsert (db,
                             "insert or replace into " BUILDIDS "_rfolo (source0, mtime, source1, dirname) values (?, ?, ?, ?);");

  char * const dirs[] = { (char*) dir.c_str(), NULL };

  struct timeval tv_start, tv_end;
  unsigned fts_scanned=0, fts_cached=0, fts_debuginfo=0, fts_executable=0, fts_rpm = 0, fts_sourcefiles=0;
  gettimeofday (&tv_start, NULL);
  
  FTS *fts = fts_open (dirs,
                       FTS_PHYSICAL /* don't follow symlinks */
                       | FTS_XDEV /* don't cross devices/mountpoints */
                       | FTS_NOCHDIR /* multithreaded */,
                       NULL);
  if (fts == NULL)
    {
      obatched(cerr) << "cannot fts_open " << dir << endl;
      return;
    }

  vector<string> directory_stack; // to allow knowledge of fts $DIR
  FTSENT *f;
  while ((f = fts_read (fts)) != NULL)
    {
      fts_scanned ++;
      if (interrupted)
        break;

      if (verbose > 3)
        obatched(clog) << "fts/rpm traversing " << f->fts_path << endl;

      try
        {
          /* Found a file.  Convert it to an absolute path, so
             the buildid database does not have relative path
             names that are unresolvable from a subsequent run
             in a different cwd. */
          char *rp = realpath(f->fts_path, NULL);
          if (rp == NULL)
            throw libc_exception(errno, "fts realpath " + string(f->fts_path));
          string rps = string(rp);
          free (rp);

          switch (f->fts_info)
            {
            case FTS_D:
              directory_stack.push_back (rps);
              break;

            case FTS_DP:
              {
                string sourcedir;
                if (directory_stack.size() == 0) // in case -R /path/to/file or FTS_D didn't line up with FTS_DP
                  sourcedir = ".";
                else
                  {
                    sourcedir = directory_stack.back ();
                    directory_stack.pop_back ();
                  }

                // join all the rfolo + bolo bits for this source directory
                sqlite3_reset (ps_upsert_bolo_rfolo_join); // to allow rebinding / reexecution
                int rc = sqlite3_bind_text (ps_upsert_bolo_rfolo_join, 1, sourcedir.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 brfjoin bind");
                rc = sqlite3_step (ps_upsert_bolo_rfolo_join);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 brfjoin execute");           

                // rollin', rollin', rollin'
                // keep those records joining'
                // ....
                // clean them up, rawhide

                sqlite3_reset (ps_bolo_nuke); // to allow rebinding / reexecution
                rc = sqlite3_bind_text (ps_bolo_nuke, 1, sourcedir.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 bolo-nuke bind");
                rc = sqlite3_step (ps_bolo_nuke);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 bolo-nuke execute");           

                sqlite3_reset (ps_rfolo_nuke); // to allow rebinding / reexecution
                rc = sqlite3_bind_text (ps_rfolo_nuke, 1, sourcedir.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 rfolo-nuke bind");
                rc = sqlite3_step (ps_rfolo_nuke);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 rfolo-nuke execute");
              }
              break;

            case FTS_F:
              {
                string sourcedir;
                if (directory_stack.size() == 0) // in case -F /path/to/file or FTS_D didn't line up with FTS_DP
                  sourcedir = ".";
                else
                  sourcedir = directory_stack.back ();
                
                // heuristic: reject if file name does not end with ".rpm"
                // (alternative: try opening with librpm etc., caching)
                string suffix = ".rpm";
                if (rps.size() < suffix.size() ||
                    rps.substr(rps.size()-suffix.size()) != suffix)
                  // !equal(rps.begin()+rps.size()-suffix.size(), rps.end(), suffix.begin()))
                  continue;
                fts_rpm ++;
                
                /* See if we know of it already. */
                sqlite3_reset (ps_query); // to allow rebinding / reexecution
                int rc = sqlite3_bind_text (ps_query, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 file query bind1");
                rc = sqlite3_bind_int64 (ps_query, 2, (int64_t) f->fts_statp->st_mtime);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 file query bind2");
                rc = sqlite3_step (ps_query);
                if (rc == SQLITE_ROW) // i.e., a result, as opposed to DONE (no results)
                  // no need to recheck a file/version we already know
                  // specifically, no need to elf-begin a file we already determined is non-elf
                  // (so is stored with buildid=NULL)
                  {
                    fts_cached ++;
                    continue;
                  }

                // register this file name in the interning table
                sqlite3_reset (ps_upsert_files);
                rc = sqlite3_bind_text (ps_upsert_files, 1, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert-file bind");           
                rc = sqlite3_step (ps_upsert_files);
                if (rc != SQLITE_OK && rc != SQLITE_DONE)
                  throw sqlite_exception(rc, "sqlite3 upsert-file execute");           
                
                // extract the rpm contents via popen("rpm2cpio") | libarchive | loop-of-elf_classify()
                unsigned my_fts_executable = 0, my_fts_debuginfo = 0, my_fts_sourcefiles = 0;
                try
                  {
                    rpm_classify (rps,
                                  ps_upsert_buildids, ps_upsert_files, ps_upsert, ps_bolo_upsert, ps_rfolo_upsert,
                                  f->fts_statp->st_mtime, sourcedir,
                                  my_fts_executable, my_fts_debuginfo, my_fts_sourcefiles);
                  }
                catch (const reportable_exception& e)
                  {
                    e.report(clog);
                  }

                fts_executable += my_fts_executable;
                fts_debuginfo += my_fts_debuginfo;
                fts_sourcefiles += my_fts_sourcefiles;
                
                // unreadable or corrupt or non-ELF-carrying rpm: cache negative
                if (my_fts_executable == 0 && my_fts_debuginfo == 0)
                  {
                    sqlite3_reset (ps_upsert); // to allow rebinding / reexecution
                    rc = sqlite3_bind_null (ps_upsert, 1); // buildid
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL bind1");
                    rc = sqlite3_bind_null (ps_upsert, 2); // artifacttype
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL bind2");
                    rc = sqlite3_bind_int64 (ps_upsert, 3, (int64_t) f->fts_statp->st_mtime); // mtime
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert bind3");           
                    rc = sqlite3_bind_text (ps_upsert, 4, rps.c_str(), -1, SQLITE_TRANSIENT); // source0
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert bind4");           
                    rc = sqlite3_bind_null (ps_upsert, 5); // source1
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL bind5");
                    rc = sqlite3_step (ps_upsert);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL execute");
                  }
              }
              break;

            case FTS_ERR:
            case FTS_NS:
              throw libc_exception(f->fts_errno, string("fts traversal ") + string(f->fts_path));

            default:
            case FTS_SL: /* NB: don't enter symbolic links into the database */
              break;
            }
        }
      catch (const reportable_exception& e)
        {
          e.report(clog);
        }
    }
  fts_close (fts);

  gettimeofday (&tv_end, NULL);
  double deltas = (tv_end.tv_sec - tv_start.tv_sec) + (tv_end.tv_usec - tv_start.tv_usec)*0.000001;
  
  if (verbose > 1)
    obatched(clog) << "fts/rpm traversed " << dir << " in " << deltas << "s, scanned=" << fts_scanned
                   << ", rpm=" << fts_rpm << ", cached=" << fts_cached << ", debuginfo=" << fts_debuginfo
                   << ", executable=" << fts_executable << ", sourcefiles=" << fts_sourcefiles << endl;
}



static void*
thread_main_scan_source_rpm_path (void* arg)
{
  string dir = string((const char*) arg);
  if (verbose > 2)
    obatched(clog) << "rpm-path scanning " << dir << endl;

  unsigned rescan_timer = 0;
  while (! interrupted)
    {
      try
        {
          if (rescan_timer == 0)
            scan_source_rpm_path (dir);
        }
      catch (const sqlite_exception& e)
        {
          obatched(cerr) << e.message << endl;
        }
      sleep (1);
      rescan_timer ++;
      if (rescan_s)
        rescan_timer %= rescan_s;
    }

  return 0;
}


////////////////////////////////////////////////////////////////////////


static void
signal_handler (int /* sig */)
{
  interrupted ++;

  // NB: don't do anything else in here
}



int
main (int argc, char *argv[])
{
  (void) setlocale (LC_ALL, "");
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);
  (void) textdomain (PACKAGE_TARNAME);

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);
  
  /* Set default values. */
  http_port = 8002;
  db_path = string(getenv("HOME") ?: "/") + string("/.dbgserver.sqlite"); /* XDG? */
  
  /* Parse and process arguments.  */
  int remaining;
  argp_program_version_hook = print_version; // this works
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER|ARGP_NO_ARGS, &remaining, NULL);
  if (remaining != argc)
      error (EXIT_FAILURE, 0,
             _("unexpected argument: %s"), argv[remaining]);
    
  (void) signal (SIGPIPE, SIG_IGN); // microhttpd can generate it incidentally, ignore
  (void) signal (SIGINT, signal_handler); // ^C
  (void) signal (SIGHUP, signal_handler); // EOF
  (void) signal (SIGTERM, signal_handler); // systemd
  
  /* Get database ready. */
  int rc;
  rc = sqlite3_open_v2 (db_path.c_str(), &db, (SQLITE_OPEN_READWRITE
                                               |SQLITE_OPEN_CREATE
                                               |SQLITE_OPEN_FULLMUTEX), /* thread-safe */
                        NULL);
  if (rc == SQLITE_CORRUPT)
    {
      (void) unlink (db_path.c_str());
      error (EXIT_FAILURE, 0,
             _("cannot open %s, deleted database: %s"), db_path.c_str(), sqlite3_errmsg(db));
    }
  else if (rc)
    {
      error (EXIT_FAILURE, 0,
             _("cannot open %s, database: %s"), db_path.c_str(), sqlite3_errmsg(db));
    }

  obatched(clog) << "Opened database " << db_path << endl;

  if (verbose > 3)
    obatched(clog) << "DDL:\n" << DBGSERVER_SQLITE_DDL << endl;
  
  rc = sqlite3_exec (db, DBGSERVER_SQLITE_DDL, NULL, NULL, NULL);
  if (rc != SQLITE_OK)
    {
      error (EXIT_FAILURE, 0,
             _("cannot run database schema ddl: %s"), sqlite3_errmsg(db));
    }

  if (verbose) // report database stats
    try
      {
        sqlite_ps ps_query (db, 
                            "select sourcetype, artifacttype, count(*) from " BUILDIDS
                            " group by sourcetype, artifacttype");

        obatched(clog) << "Database statistics:" << endl;
        obatched(clog) << "source" << "\t" << "type" << "\t" << "count" << endl;
        while (1)
          {
            rc = sqlite3_step (ps_query);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW)
              throw sqlite_exception(rc, "step");

            obatched(clog) << (sqlite3_column_text(ps_query, 0) ?: (const unsigned char*) "NULL")
                           << "\t"
                           << (sqlite3_column_text(ps_query, 1) ?: (const unsigned char*) "NULL")
                           << "\t"
                           << (sqlite3_column_text(ps_query, 2) ?: (const unsigned char*) "NULL")
                           << endl;
          }
      }
    catch (const reportable_exception& e)
      {
        e.report(clog);
      }
  
  for (auto&& it : source_file_paths)
    {
      pthread_t pt;
      int rc = pthread_create (& pt, NULL, thread_main_scan_source_file_path, (void*) it.c_str());
      if (rc < 0)
        error (0, 0, "Warning: cannot spawn thread (%d) to scan source files %s\n", rc, it.c_str());
      else
        source_file_scanner_threads.push_back(pt);
    }

  for (auto&& it : source_rpm_paths)
    {
      pthread_t pt;
      int rc = pthread_create (& pt, NULL, thread_main_scan_source_rpm_path, (void*) it.c_str());
      if (rc < 0)
        error (0, 0, "Warning: cannot spawn thread (%d) to scan source rpms %s\n", rc, it.c_str());
      else
        source_rpm_scanner_threads.push_back(pt);
    }

  
  // Start httpd server threads.  Separate pool for IPv4 and IPv6, in
  // case the host only has one protocol stack.
  MHD_Daemon *d4 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION
#if MHD_VERSION >= 0x00095300
                                     | MHD_USE_INTERNAL_POLLING_THREAD
#else
                                     | MHD_USE_SELECT_INTERNALLY
#endif
                                     | MHD_USE_DEBUG, /* report errors to stderr */
                                     http_port,
                                     NULL, NULL, /* default accept policy */
                                     handler_cb, NULL, /* handler callback */
                                     MHD_OPTION_END);
  MHD_Daemon *d6 = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION
#if MHD_VERSION >= 0x00095300
                                     | MHD_USE_INTERNAL_POLLING_THREAD
#else
                                     | MHD_USE_SELECT_INTERNALLY
#endif
                                     | MHD_USE_IPv6
                                     | MHD_USE_DEBUG, /* report errors to stderr */
                                     http_port,
                                     NULL, NULL, /* default accept policy */
                                     handler_cb, NULL, /* handler callback */
                                     MHD_OPTION_END);

  if (d4 == NULL && d6 == NULL) // neither ipv4 nor ipv6? boo
    {
      sqlite3_close (db);
      error (EXIT_FAILURE, 0, _("cannot start http server at port %d"), http_port);
    }

  obatched(clog) << "Started http server on "
                 << (d4 != NULL ? "IPv4 " : "")
                 << (d6 != NULL ? "IPv6 " : "")
                 << "port=" << http_port << endl;

  const char* du = getenv("DBGSERVER_URLS");
  if (du && du[0] != '\0') // set to non-empty string?
    obatched(clog) << "Upstream dbgservers: " << du << endl;
  
  /* Trivial main loop! */
  while (! interrupted)
    pause ();

  if (verbose)
    obatched(clog) << "Stopping" << endl;
  
  /* Stop all the web service threads. */
  if (d4) MHD_stop_daemon (d4);
  if (d6) MHD_stop_daemon (d6);
  
  /* Join any source scanning threads. */
  for (auto&& it : source_file_scanner_threads)
    pthread_join (it, NULL);
  for (auto&& it : source_rpm_scanner_threads)
    pthread_join (it, NULL);
  
  /* With all threads known dead, we can close the db handle. */
  sqlite3_close (db);
  
  return 0;
}
