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

#include "printversion.h"

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
#include <string>
#include <iostream>
#include <ostream>
#include <sstream>
using namespace std;

#include <gelf.h>
#include <libdwelf.h>

#include <microhttpd.h>
#include <curl/curl.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmarchive.h>
#include <sqlite3.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

#ifndef _
# define _(str) gettext (str)
#endif


// Roll this identifier for every sqlite schema incompatiblity
// XXX: garbage collect and/or migrate from previous-version tables
#define BUILDIDS "buildids1"

static const char DBGSERVER_SQLITE_DDL[] =
  "create table if not exists\n"
  "    " BUILDIDS "(\n"
  "        buildid text,                                   -- the buildid; null=negative-cache\n"
  "        artifacttype text(1)\n"
  "            check (artifacttype IS NULL OR artifacttype IN ('D', 'S', 'E')),    -- d(ebug) or s(sources) or e(xecutable)\n"
  "        mtime integer not null,                         -- epoch timestamp when we last found this\n"
  "        sourcetype text(1) not null\n"
  "            check (sourcetype IN ('F', 'R', 'L')),      -- as per --source-TYPE single-char code\n"
  "        source0 text,                                   -- more sourcetype-specific location data\n"
  "        source1 text);                                  -- more sourcetype-specific location data\n"
  "create index if not exists " BUILDIDS "_idx1 on " BUILDIDS " (buildid, artifacttype);\n"
  "create unique index if not exists " BUILDIDS "_idx2 on " BUILDIDS " (buildid, artifacttype, sourcetype, source0);\n"
  "create index if not exists " BUILDIDS "_idx3 on " BUILDIDS " (sourcetype, source0);\n";

// schema change history
//
// buildid1: make buildid and artifacttype NULLable, to represent cached-negative
//           lookups from sources, e.g. files or rpms that contain no buildid-indexable content
//
// buildid: original




/*
  ISSUES:
  - delegated server: recursion/loop; Via: header processing
  https://blog.cloudflare.com/preventing-malicious-request-loops/
  - cache control for downloaded data ===>> no problem, we don't download & store
  - access control ===>> delegate to reverse proxy
  - running test server on fedorainfra, scanning koji rpms
  - running real server for rhel/rhsm probably unnecessary
  (use subscription-delegation)
  - upstream: support http proxy for relay mode ===> $env(http_proxy) in libcurl
  - expose main executable elf, not just dwarf ===> ok
  - need a thread to garbage-collect old buildid entries?

  - cache eperm file opens ("cannot open FOO" -> F NULL-buildid ?); age/retry by mtime
  - print proper sqlite3 / elfutils errors
  - when passing compressed .ko.xz's by content (not by name!), will elfutils client know to decompress?
  - cmdline single-char -X parsing
  - database schema migration - suffix buildid table name with seq#, select * at startup to migrate?
  - inotify based file scanning

  see also:
  https://github.com/NixOS/nixos-channel-scripts/blob/master/index-debuginfo.cc
  https://github.com/edolstra/dwarffs
*/


/* Name and version of program.  */
/* ARGP_PROGRAM_VERSION_HOOK_DEF = print_version; */

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
  {
   { NULL, 0, NULL, 0, N_("Sources:"), 1 },
   { "source-files", 'F', "PATH", 0, N_("Scan ELF/DWARF files under given directory."), 0 },
   { "source-rpms", 'R', "PATH", 0, N_("Scan RPM files under given directory."), 0 },
   { "source-rpms-yum", 0, "SECONDS", 0, N_("Try fetching missing RPMs from yum."), 0 },
   // "source-rpms-koji"      ... no can do, not buildid-addressable
   // "source-imageregistry"  ... 
   // "source-debs"           ... future
   { "source-redirect", 'U', "URL", 0, N_("Redirect to upstream dbgserver."), 0 },
   { "source-relay", 'u', "URL", 0, N_("Relay from upstream dbgserver."), 0 },
  
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
static vector<string> source_file_paths;
static vector<pthread_t> source_file_scanner_threads;
static vector<string> source_rpm_paths;
static vector<pthread_t> source_rpm_scanner_threads;
static unsigned source_file_rescan_time = 300; /* XXX: parametrize */



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
    case 'F': source_file_paths.push_back(string(arg)); break;
    case 't': rescan_s = atoi(arg); break;
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
    int rc = sqlite3_prepare_v2 (db, sql.c_str(), -1 /* to \0 */, & this->pp, NULL);
    if (rc != SQLITE_OK)
      throw sqlite_exception(rc, "prepare " + sql);
  }
  ~sqlite_ps () { sqlite3_finalize (this->pp); }
  operator sqlite3_stmt* () { return this->pp; }
};



////////////////////////////////////////////////////////////////////////


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


static struct MHD_Response*
handle_buildid_match (int64_t b_mtime,
                      const string& b_stype,
                      const string& b_source0,
                      const string& b_source1)
{
  if (b_stype != "F")
    {
      if (verbose > 2)
        obatched(clog) << "unimplemented stype " << b_stype << endl;
      return 0;
    }
  
  int fd = open(b_source0.c_str(), O_RDONLY);
  if (fd < 0)
    {
      if (verbose > 2)
        obatched(clog) << "cannot open " << b_source0 << endl;
      // XXX: delete the buildid record?
      // NB: it is safe to delete while a select loop is under way
      return 0;
    }
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
    if (verbose)
      obatched(clog) << "serving file " << b_source0 << endl;
    /* libmicrohttpd will close it. */
    ;

  return r;
}


static struct MHD_Response* handle_buildid (struct MHD_Connection *connection,
                const string& buildid /* unsafe */,
                const string& artifacttype /* unsafe */,
                const string& suffix /* unsafe */)
{
  string atype_code;
  if (artifacttype == "debuginfo") atype_code = "D";
  else if (artifacttype == "executable") atype_code = "E";
  else if (artifacttype == "source-file") atype_code = "S";
  else throw reportable_exception("invalid artifacttype");

  if (verbose > 0)
    obatched(clog) << "searching for buildid=" << buildid << " artifacttype=" << artifacttype
         << " suffix=" << suffix << endl;
  
  sqlite_ps pp (db, 
                "select mtime, sourcetype, source0, source1 " // NB: 4 columns
                "from " BUILDIDS " where buildid = ? and artifacttype = ? "
                "order by mtime desc;");

  int rc = sqlite3_bind_text (pp, 1, buildid.c_str(), -1 /* to \0 */, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
    throw sqlite_exception(rc, "bind 1");
  rc = sqlite3_bind_text (pp, 2, atype_code.c_str(), -1 /* to \0 */, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
      throw sqlite_exception(rc, "bind 2");

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

  throw reportable_exception(403, "not found");
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
  char errmsg[100] = "";
  struct MHD_Response *r = NULL;
  string url_copy = url;
  char *tok = NULL;
  char *tok_save = NULL;
  
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
              suffix = url_copy.substr(slash3+1);
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
      return e.mhd_send_response (connection);
    }
}


////////////////////////////////////////////////////////////////////////


static void
elf_classify (int fd, bool &executable_p, bool &debuginfo_p, string &buildid)
{
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP_PRIVATE, NULL);
  if (elf == NULL)
    return;

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
      elf_end (elf);
      return;
    }
  // build_id is a raw byte array; convert to hexadecimal
  unsigned char* build_id_bytes = (unsigned char*) build_id;
  for (ssize_t idx=0; idx<sz; idx++)
    {
      buildid += "0123456789abcdef"[build_id_bytes[idx] >> 4];
      buildid += "0123456789abcdef"[build_id_bytes[idx] & 0xf];
    }

  // now decide whether it's an executable
  if (elf_type == ET_EXEC || elf_type == ET_DYN)
    executable_p = true;

  // now decide whether it's a debuginfo - namely, if it has any .debug* or .zdebug* sections
  // logic mostly stolen from fweimer@redhat.com's elfclassify drafts
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    {
      elf_end (elf);
      return;
    }
  
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
      if (strncmp(section_name, ".debug_", 7) == 0 ||
          strncmp(section_name, ".zdebug_", 8) == 0)
        {
          debuginfo_p = true;
          break;
        }
    }

 elfend:  
  elf_end (elf);
}



static void
scan_source_file_path (const string& dir)
{
  sqlite_ps ps_upsert (db,
                       "insert or ignore into " BUILDIDS " (buildid, artifacttype, mtime,"
                       "sourcetype, source0) values (?, ?, ?, 'F', ?);");
  sqlite_ps ps_query (db,
                      "select 1 from " BUILDIDS " where sourcetype = 'F' and source0 = ? and mtime = ?;");

  char * const dirs[] = { (char*) dir.c_str(), NULL };

  struct timeval tv_start, tv_end;
  unsigned fts_scanned=0, fts_cached=0, fts_debuginfo=0, fts_executable=0;
  gettimeofday (&tv_start, NULL);
  
  FTS *fts = fts_open (dirs, FTS_PHYSICAL | FTS_NOCHDIR /* multithreaded */, NULL);
  if (fts == NULL)
    {
      obatched(cerr) << "cannot fts_open " << dir << endl;
      return;
    }

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
          switch (f->fts_info)
            {
            case FTS_F:
            case FTS_SL:
              {
                /* Found a file.  Convert it to an absolute path, so
                   the buildid database does not have relative path
                   names that are unresolvable from a subsequent run
                   in a different cwd. */
                char *rp = realpath(f->fts_path, NULL);
                if (rp == NULL)
                  throw libc_exception(errno, "fts realpath");
                string rps = string(rp);
                free (rp);
                
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

                int fd = open (rps.c_str(), O_RDONLY);
                if (fd < 0)
                  // XXX: cache this as a negative-hit null-build case too?
                  throw libc_exception(errno, string("opening ") + rps);
                 

                bool executable_p = false, debuginfo_p = false; // E and/or D
                string buildid;
                elf_classify (fd, executable_p, debuginfo_p, buildid);
                close (fd);
                
                sqlite3_reset (ps_upsert); // to allow rebinding / reexecution

                if (buildid == "")
                  {
                    rc = sqlite3_bind_null (ps_upsert, 1);
                    // no point storing an elf file without buildid
                    executable_p = false;
                    debuginfo_p = false;
                  }
                else
                  rc = sqlite3_bind_text (ps_upsert, 1, buildid.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind1");           
                rc = sqlite3_bind_int64 (ps_upsert, 3, (int64_t) f->fts_statp->st_mtime);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind3");           
                rc = sqlite3_bind_text (ps_upsert, 4, rps.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                  throw sqlite_exception(rc, "sqlite3 upsert bind4");           
          
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

                if (! (executable_p || debuginfo_p))
                  {
                    rc = sqlite3_bind_null (ps_upsert, 2);
                    if (rc != SQLITE_OK)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL bind2");
                    rc = sqlite3_step (ps_upsert);
                    if (rc != SQLITE_OK && rc != SQLITE_DONE)
                      throw sqlite_exception(rc, "sqlite3 upsert-NULL execute");
                  }
                
                if (verbose > 2)
                  obatched(clog) << "recorded buildid=" << buildid << " file=" << rps
                                 << " mtime=" << f->fts_statp->st_mtime << " as "
                                 << (executable_p ? "executable" : "not executable") << " and "
                                 << (debuginfo_p ? "debuginfo" : "not debuginfo") << endl;
              }
              break;

            case FTS_ERR:
            case FTS_NS:
              throw libc_exception(f->fts_errno, string("fts traversal ") + string(f->fts_path));

            default:
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
                   << ", executable=" << fts_executable << endl;
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
      rescan_timer = (rescan_timer + 1) % rescan_s;
    }
  
  return 0;
}


static void*
thread_main_scan_source_rpm_path (void* arg)
{
  string dir = string((const char*) arg);
  if (verbose > 2)
    obatched(clog) << "rpm-path scanning " << dir << endl;

  while (! interrupted)
    {
      try
        {
          // XXX scan_source_rpm_path (dir);
          sleep (10); // parametrize
        }
      catch (const sqlite_exception& e)
        {
          obatched(cerr) << e.message << endl;
        }
    }
  
  return 0;
}


////////////////////////////////////////////////////////////////////////


static void
signal_handler (int /* sig */)
{
  interrupted ++;
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
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &remaining, NULL);

  /* Block SIGPIPE, as libmicrohttpd operations can trigger it, and we don't care. */
  (void) signal (SIGPIPE, SIG_IGN);
  (void) signal (SIGINT, signal_handler);
  (void) signal (SIGHUP, signal_handler);
  
  /* Get database ready. */
  int rc;
  rc = sqlite3_open_v2 (db_path.c_str(), &db, (SQLITE_OPEN_READWRITE
                                               |SQLITE_OPEN_CREATE
                                               |SQLITE_OPEN_FULLMUTEX), /* thread-safe */
                        NULL);
  if (rc)
    {
      error (EXIT_FAILURE, 0,
             _("cannot open database: %s"), sqlite3_errmsg(db));
    }

  rc = sqlite3_exec (db, DBGSERVER_SQLITE_DDL, NULL, NULL, NULL);
  if (rc != SQLITE_OK)
    {
      error (EXIT_FAILURE, 0,
             _("cannot run database schema ddl: %s"), sqlite3_errmsg(db));
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

  
  /* Start httpd server threads. */
  /* XXX: suppress SIGPIPE */
  MHD_Daemon *daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION
                                         | MHD_USE_INTERNAL_POLLING_THREAD
                                         | MHD_USE_DUAL_STACK /* ipv4 + ipv6 */
                                         | MHD_USE_DEBUG, /* report errors to stderr */
                             http_port,
                             NULL, NULL, /* default accept policy */
                             handler_cb, NULL, /* handler callback */
                             MHD_OPTION_END);
  if (daemon == NULL)
    {
      sqlite3_close (db);
      error (EXIT_FAILURE, 0, _("cannot start http server at port %p"), http_port);
    }

  if (verbose)
    obatched(clog) << "Started http server on port=" << http_port
                   << ", database path=" << db_path
                   << ", rescan time=" << rescan_s
                   << endl;
  
  /* Trivial main loop! */
  while (! interrupted)
    pause ();

  if (verbose)
    obatched(clog) << "Stopping" << endl;
  
  /* Stop all the web service threads. */
  /* MHD_quiesce_daemon (daemon); */
  MHD_stop_daemon (daemon);

  /* Join any source scanning threads. */
  for (auto&& it : source_file_scanner_threads)
    pthread_join (it, NULL);
  for (auto&& it : source_rpm_scanner_threads)
    pthread_join (it, NULL);
  
  /* With all threads known dead, we can close the db handle. */
  sqlite3_close (db);
  
  return 0;
}
