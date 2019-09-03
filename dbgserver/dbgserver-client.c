/* Retrieve ELF / DWARF / source files from the dbgserver.
   Copyright (C) 2019 Red Hat, Inc.
   This file is part of elfutils.

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

#include "config.h"
#include "dbgserver-client.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <string.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <time.h>
#include <utime.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>

static const int max_build_id_bytes = 256; /* typical: 40 for gnu C toolchain */


/* The cache_clean_interval_s file within the dbgserver cache specifies
   how frequently the cache should be cleaned. The file's st_mtime represents
   the time of last cleaning.  */
static const char *cache_clean_interval_filename = "cache_clean_interval_s";
static const time_t cache_clean_default_interval_s = 600;

/* Location of the cache of files downloaded from dbgservers.
   The default parent directory is $HOME, or '/' if $HOME doesn't exist.  */
static const char *cache_default_name = ".dbgserver_client_cache";
static const char *cache_path_envvar = DBGSERVER_CACHE_PATH_ENV_VAR;

/* URLs of dbgservers, separated by url_delim.
   This env var must be set for dbgserver-client to run.  */
static const char *server_urls_envvar = DBGSERVER_URLS_ENV_VAR;
static const char *url_delim =  " ";

/* Timeout for dbgservers, in seconds.
   This env var must be set for dbgserver-client to run.  */
static const char *server_timeout_envvar = DBGSERVER_TIMEOUT_ENV_VAR;
static int server_timeout = 5;


static size_t
dbgserver_write_callback (char *ptr, size_t size, size_t nmemb, void *fdptr)
{
  int fd = *(int*)fdptr;
  ssize_t res;
  ssize_t count = size * nmemb;

  res = write(fd, (void*)ptr, count);
  /* XXX: can we just return res? */
  if (res < 0)
    return (size_t)0;

  return (size_t)res;
}



/* Create the cache and interval file if they do not already exist.
   Return DBGSERVER_E_OK if cache and config file are initialized,
   otherwise return the appropriate error code.  */
static int
dbgserver_init_cache (char *cache_path, char *interval_path)
{
  struct stat st;

  /* If the cache and config file already exist then we are done.  */
  if (stat(cache_path, &st) == 0 && stat(interval_path, &st) == 0)
    return 0;

  /* Create the cache and config file as necessary.  */
  if (stat(cache_path, &st) != 0 && mkdir(cache_path, 0777) < 0)
    return -errno;

  int fd;
  if (stat(interval_path, &st) != 0
      && (fd = open(interval_path, O_CREAT | O_RDWR, 0666)) < 0)
    return -errno;

  /* write default interval to config file.  */
  if (dprintf(fd, "%ld", cache_clean_default_interval_s) < 0)
    return -errno;

  return 0;
}


/* Delete any files that have been unmodied for a period
   longer than $DBGSERVER_CACHE_CLEAN_INTERVAL_S.  */
static int
dbgserver_clean_cache(char *cache_path, char *interval_path)
{
  struct stat st;
  FILE *interval_file;

  if (stat(interval_path, &st) == -1)
    {
      /* Create new interval file.  */
      interval_file = fopen(interval_path, "w");

      if (interval_file == NULL)
        return -errno;

      int rc = fprintf(interval_file, "%ld", cache_clean_default_interval_s);
      fclose(interval_file);

      if (rc < 0)
        return -errno;
    }

  /* Check timestamp of interval file to see whether cleaning is necessary.  */
  time_t clean_interval;
  interval_file = fopen(interval_path, "r");
  if (fscanf(interval_file, "%ld", &clean_interval) != 1)
    clean_interval = cache_clean_default_interval_s;
  fclose(interval_file);

  if (time(NULL) - st.st_mtime < clean_interval)
    /* Interval has not passed, skip cleaning.  */
    return 0;

  char * const dirs[] = { cache_path, NULL, };

  FTS *fts = fts_open(dirs, 0, NULL);
  if (fts == NULL)
    return -errno;

  FTSENT *f;
  while ((f = fts_read(fts)) != NULL)
    {
      switch (f->fts_info)
        {
        case FTS_F:
          /* delete file if cache clean interval has been met or exceeded.  */
          /* XXX: ->st_mtime is the wrong metric.  We'd want to track -usage- not the mtime, which 
             we copy from the http Last-Modified: header, and represents the upstream file's mtime. */
          /* XXX clean_interval should be a separate parameter max_unused_age */
          /* XXX consider extra effort to clean up old tmp files */
          if (time(NULL) - f->fts_statp->st_mtime >= clean_interval)
            unlink (f->fts_path);
          break;

        case FTS_DP:
          /* Remove if empty. */
          (void) rmdir (f->fts_path);
          break;
          
        default:
          ;
        }
    }
  fts_close(fts);

  /* Update timestamp representing when the cache was last cleaned.  */
  utime (interval_path, NULL);
  return 0;
}



/* Query each of the server URLs found in $DBGSERVER_URLS for the file
   with the specified build-id, type (debuginfo, executable or source)
   and filename. filename may be NULL. If found, return a file
   descriptor for the target, otherwise return an error code.  */
static int
dbgserver_query_server (const unsigned char *build_id,
                        int build_id_len,
                        const char *type,
                        const char *filename,
                        char **path)
{
  char *urls_envvar;
  char *server_urls;
  char cache_path[PATH_MAX];
  char interval_path[PATH_MAX];
  char target_cache_dir[PATH_MAX];
  char target_cache_path[PATH_MAX];
  char target_cache_tmppath[PATH_MAX];
  char suffix[PATH_MAX];
  char build_id_bytes[max_build_id_bytes * 2 + 1];

  /* Copy lowercase hex representation of build_id into buf.  */
  if ((build_id_len >= max_build_id_bytes) ||
      (build_id_len == 0 &&
       strlen((const char*) build_id_bytes) >= max_build_id_bytes*2))
    return -EINVAL;
  if (build_id_len == 0) /* expect clean hexadecimal */
    strcpy (build_id_bytes, (const char *) build_id);
  else
    for (int i = 0; i < build_id_len; i++)
      sprintf(build_id_bytes + (i * 2), "%02x", build_id[i]);

  unsigned q = 0;
  if (filename != NULL)
    {
      if (filename[0] != '/') // must start with /
        return -EINVAL;

      /* copy the filename to suffix, s,/,#,g */
      for (q=0; q<sizeof(suffix)-1; q++)
        {
          if (filename[q] == '\0') break;
          if (filename[q] == '/' || filename[q] == '.') suffix[q] = '#';
          else suffix[q] = filename[q];
        }
      /* XXX: if we had a CURL* handle at this time, we could
         curl_easy_escape() to url-escape the filename in a
         collision-free, reversible manner. */
    }
  suffix[q] = '\0';
  
  /* set paths needed to perform the query

     example format
     cache_path:        $HOME/.dbgserver_cache
     target_cache_dir:  $HOME/.dbgserver_cache/0123abcd
     target_cache_path: $HOME/.dbgserver_cache/0123abcd/debuginfo
     target_cache_path: $HOME/.dbgserver_cache/0123abcd/source#PATH#TO#SOURCE ?
  */
  
  if (getenv(cache_path_envvar))
    strcpy(cache_path, getenv(cache_path_envvar));
  else
    {
      if (getenv("HOME"))
        sprintf(cache_path, "%s/%s", getenv("HOME"), cache_default_name);
      else
        sprintf(cache_path, "/%s", cache_default_name);
    }

  /* avoid using snprintf here due to compiler warning.  */
  snprintf(target_cache_dir, PATH_MAX, "%s/%s", cache_path, build_id_bytes);
  snprintf(target_cache_path, PATH_MAX, "%s/%s%s", target_cache_dir, type, suffix);
  snprintf(target_cache_tmppath, PATH_MAX, "%s.XXXXXX", target_cache_path);

  /* XXX combine these */
  snprintf(interval_path, PATH_MAX, "%s/%s", cache_path, cache_clean_interval_filename);
  int rc = dbgserver_init_cache(cache_path, interval_path);
  if (rc != 0)
    goto out;
  rc = dbgserver_clean_cache(cache_path, interval_path);
  if (rc != 0)
    goto out;

  
  /* If the target is already in the cache then we are done.  */
  int fd = open (target_cache_path, O_RDONLY);
  if (fd >= 0)
    {
      /* Success!!!! */
      if (path != NULL)
        *path = strdup(target_cache_path);
      return fd;
    }


  urls_envvar = getenv(server_urls_envvar);
  if (urls_envvar == NULL)
    {
      rc = -ENOSYS;
      goto out;
    }

  if (getenv(server_timeout_envvar))
    server_timeout = atoi (getenv(server_timeout_envvar));
  
  /* make a copy of the envvar so it can be safely modified.  */
  server_urls = strdup(urls_envvar);
  if (server_urls == NULL)
    {
      rc = -ENOMEM;
      goto out;
    }
  /* thereafter, goto out0 on error*/

  /* create target directory in cache if not found.  */
  struct stat st;
  if (stat(target_cache_dir, &st) == -1 && mkdir(target_cache_dir, 0700) < 0)
    {
      rc = -errno;
      goto out0;
    }

  /* NB: write to a temporary file first, to avoid race condition of
     multiple clients checking the cache, while a partially-written or empty
     file is in there, being written from libcurl. */
  fd = mkstemp (target_cache_tmppath);
  if (fd < 0)
    {
      rc = -errno;
      goto out0;
    }

  /* thereafter, goto out1 on error */
  
  CURL *session = curl_easy_init();
  if (session == NULL)
    {
      rc = -ENETUNREACH;
      goto out1;
    }
  /* thereafter, goto out2 on error */

  char *strtok_saveptr;
  char *server_url = strtok_r(server_urls, url_delim, &strtok_saveptr);
  /* Try the various servers sequentially.  XXX: in parallel instead. */
  while (server_url != NULL)
    {
      /* query servers until we find the target or run out of urls to try.  */
      char url[PATH_MAX];

      /* Tolerate both   http://foo:999  and http://foo:999/  forms */
      char *slashbuildid;
      if (strlen(server_url) > 1 && server_url[strlen(server_url)-1] == '/')
        slashbuildid = "buildid";
      else
        slashbuildid = "/buildid";
      
      if (filename) /* must start with / */
        snprintf(url, PATH_MAX, "%s%s/%s/%s%s", server_url,
                 slashbuildid, build_id_bytes, type, filename);
      else
        snprintf(url, PATH_MAX, "%s%s/%s/%s", server_url,
                 slashbuildid, build_id_bytes, type);

      curl_easy_reset(session);
      curl_easy_setopt(session, CURLOPT_URL, url);
      curl_easy_setopt(session,
                       CURLOPT_WRITEFUNCTION,
                       dbgserver_write_callback);
      curl_easy_setopt(session, CURLOPT_WRITEDATA, (void*)&fd);
      curl_easy_setopt(session, CURLOPT_TIMEOUT, (long) server_timeout);
      curl_easy_setopt(session, CURLOPT_FILETIME, (long) 1);
      curl_easy_setopt(session, CURLOPT_FOLLOWLOCATION, (long) 1);
      curl_easy_setopt(session, CURLOPT_FAILONERROR, (long) 1);      
      curl_easy_setopt(session, CURLOPT_AUTOREFERER, (long) 1);
      curl_easy_setopt(session, CURLOPT_ACCEPT_ENCODING, "");
      curl_easy_setopt(session, CURLOPT_USERAGENT, (void*) PACKAGE_STRING);
      
      CURLcode curl_res = curl_easy_perform(session);
      if (curl_res != CURLE_OK)
        {
          /* curl_easy_getinfo(session, CURLINFO_OS_ERRNO ...) would be nice if it worked. */
          switch (curl_res) /* map CURL error numbers to approximate libc errnos */
            {
            case CURLE_COULDNT_RESOLVE_HOST: rc = -EHOSTUNREACH; break; // no NXDOMAIN
            case CURLE_URL_MALFORMAT: rc = -EINVAL; break;
            case CURLE_COULDNT_CONNECT: rc = -ECONNREFUSED; break;
            case CURLE_REMOTE_ACCESS_DENIED: rc = -EACCES; break;
            case CURLE_WRITE_ERROR: rc = -EIO; break;
            case CURLE_OUT_OF_MEMORY: rc = -ENOMEM; break;
            case CURLE_TOO_MANY_REDIRECTS: rc = -EMLINK; break;
            case CURLE_SEND_ERROR: rc = -ECONNRESET; break;
            case CURLE_RECV_ERROR: rc = -ECONNRESET; break;
            default: rc = -ENOENT; break;
            }
          
          server_url = strtok_r(NULL, url_delim,&strtok_saveptr);
          continue; /* fail over to next server */
        }

      long resp_code = 500;
      curl_res = curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &resp_code);
      if ((curl_res != CURLE_OK) || (resp_code != 200))
        {
          server_url = strtok_r(NULL, url_delim,&strtok_saveptr);
          continue;
        }

      time_t mtime;
      curl_res = curl_easy_getinfo(session, CURLINFO_FILETIME, (void*) &mtime);
      if (curl_res != CURLE_OK)
        mtime = time(NULL); /* fall back to current time */
        
      /* we've got one!!!! */
      struct timeval tvs[2];
      tvs[0].tv_sec = tvs[1].tv_sec = mtime;
      tvs[0].tv_usec = tvs[1].tv_usec = 0;
      (void) futimes (fd, tvs);  /* best effort */
          
      /* rename tmp->real */
      rc = rename (target_cache_tmppath, target_cache_path);
      if (rc < 0)
        {
          rc = -errno;
          goto out2;
          /* Perhaps we need not give up right away; could retry or something ... */
        }

      /* Success!!!! */
      curl_easy_cleanup(session);
      free (server_urls);
      /* don't close fd - we're returning it */
      /* don't unlink the tmppath; it's already been renamed. */
      if (path != NULL)
        *path = strdup(target_cache_path);
      return fd;
    }

  /* fell through - out of alternative servers */
  /* prefer to preserve the last rc set from curl OS_ERRNO */
  if (rc == 0)
    rc = -ENOENT;

/* error exits */
 out2:
  curl_easy_cleanup(session);
  
 out1:
  unlink (target_cache_tmppath);
  (void) rmdir (target_cache_dir); /* nop if not empty */
  close (fd);

 out0:
  free (server_urls);
  
 out:
  return rc;
}


/* See dbgserver-client.h  */
int
dbgserver_find_debuginfo (const unsigned char *build_id, int build_id_len,
                          char **path)
{
  return dbgserver_query_server(build_id, build_id_len,
                                "debuginfo", NULL, path);
}


/* See dbgserver-client.h  */
int
dbgserver_find_executable(const unsigned char *build_id, int build_id_len,
                          char **path)
{
  return dbgserver_query_server(build_id, build_id_len,
                                "executable", NULL, path);
}

/* See dbgserver-client.h  */
int dbgserver_find_source(const unsigned char *build_id,
                          int build_id_len,
                          const char *filename,
                          char **path)
{
  return dbgserver_query_server(build_id, build_id_len,
                                "source", filename, path);
}



/* NB: these are thread-unsafe. */
__attribute__((constructor)) attribute_hidden void libdbgserver_ctor(void) 
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

/* NB: this is very thread-unsafe: it breaks other threads that are still in libcurl */
__attribute__((destructor)) attribute_hidden void libdbgserver_dtor(void) 
{
  /* ... so don't do this: */
  /* curl_global_cleanup(); */
}
