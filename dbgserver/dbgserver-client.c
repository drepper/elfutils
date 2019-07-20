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

static const int max_build_id_bytes = 64;
static int DBGCLIENT_OK = 0;

/* The cache_clean_interval_s file within the dbgclient cache specifies
   how frequently the cache should be cleaned. The file's st_mtime represents
   the time of last cleaning.  */
static const char *cache_clean_interval_filename = "cache_clean_interval_s";
static const time_t cache_clean_default_interval_s = 600;

/* Location of the cache of files downloaded from dbgservers.
   The default parent directory is $HOME, or '/' if $HOME doesn't exist.  */
static const char *cache_default_name = ".dbgserver_client_cache";
static const char *cache_path_envvar = "DBGSERVER_CACHE_PATH";

/* URLs of dbgservers, separated by url_delim.
   This env var must be set for dbgserver-client to run.  */
static const char *server_urls_envvar = "DBGSERVER_URLS";
static const char *url_delim =  " ";


static size_t
dbgclient_write_callback (char *ptr, size_t size, size_t nmemb, void *fdptr)
{
  int fd = *(int*)fdptr;
  ssize_t res;
  ssize_t count = size * nmemb;

  res = write(fd, (void*)ptr, count);
  if (res < 0)
    return (size_t)0;

  return (size_t)res;
}


static int
dbgclient_get_file_from_cache (char *target_cache_path)
{
  int fd;
  struct stat st;

  if (stat(target_cache_path, &st) == -1)
    return -ENOENT;

  if ((fd = open(target_cache_path, O_RDONLY)) < 0)
    return -errno;

  return fd;
}


/* Create the cache and interval file if they do not already exist.
   Return DBGCLIENT_E_OK if cache and config file are initialized,
   otherwise return the appropriate error code.  */
static int
dbgclient_init_cache (char *cache_path, char *interval_path)
{
  struct stat st;

  /* If the cache and config file already exist then we are done.  */
  if (stat(cache_path, &st) == 0 && stat(interval_path, &st) == 0)
    return DBGCLIENT_OK;

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

  return DBGCLIENT_OK;
}


/* Create a new cache entry. If successful return its file descriptor,
   otherwise return a dbgclient error code.  */
static int
dbgclient_add_file_to_cache (char *target_cache_dir,
                             char *target_cache_path)
{
  int fd;
  struct stat st;

  /* create target directory in cache if not found.  */
  if (stat(target_cache_dir, &st) == -1 && mkdir(target_cache_dir, 0777) < 0)
    return -errno;

  /* create target file if not found.  */
  if((fd = open(target_cache_path, O_CREAT | O_RDWR, 0666)) < 0)
    return -errno;

  return fd;
}


/* Delete any files that have been unmodied for a period
   longer than $DBGSERVER_CACHE_CLEAN_INTERVAL_S.  */
static int
dbgclient_clean_cache(char *cache_path, char *interval_path)
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
    return DBGCLIENT_OK;

  char * const dirs[] = { cache_path, NULL, };

  FTS *fts = fts_open(dirs, 0, NULL);
  if (fts == NULL)
    return -errno;

  FTSENT *f;
  DIR *d;
  while ((f = fts_read(fts)) != NULL)
    {
      switch (f->fts_info)
        {
        case FTS_F:
          /* delete file if cache clean interval has been met or exceeded.  */
          if (time(NULL) - f->fts_statp->st_mtime >= clean_interval)
            remove(f->fts_path);
          break;

        case FTS_DP:
          d = opendir(f->fts_path);
          /* delete directory if it doesn't contain files besides . and ..  */
          (void) readdir(d);
          (void) readdir(d);
          if (readdir(d) == NULL)
            remove(f->fts_path);
          closedir(d);
          break;

        default:
          ;
        }
    }
  fts_close(fts);

  /* Update timestamp representing when the cache was last cleaned.  */
  utime(interval_path, NULL);
  return DBGCLIENT_OK;
}


/* Return value must be manually free'd.  */
static char *
build_url(const char *server_url, const char *build_id,
          const char *type, const char *filename)
{
  char *url;

  if (filename != NULL)
    {
      url = malloc(strlen(server_url)
                   + strlen("/buildid///")
                   + strlen(build_id)
                   + strlen(type)
                   + strlen(filename)
                   + 1);

      if (url == NULL)
        return NULL;

      sprintf(url,
              "%s/buildid/%s/%s/%s",
              server_url,
              build_id,
              type,
              filename);
    }
  else
    {
      url = malloc(strlen(server_url)
                   + strlen("/buildid//")
                   + strlen(build_id)
                   + strlen(type)
                   + 1);

      if (url == NULL)
        return NULL;

      sprintf(url, "%s/buildid/%s/%s", server_url, build_id, type);
    }

  return url;
}


/* Query each of the server URLs found in $DBGSERVER_URLS for the file
   with the specified build-id, type (debuginfo, executable or source)
   and filename. filename may be NULL. If found, return a file
   descriptor for the target, otherwise return an error code.  */
static int
dbgclient_query_server (const unsigned char *build_id_bytes,
                        int build_id_len,
                        const char *type,
                        const char *filename)
{
  char *urls_envvar;
  char *server_urls;
  char cache_path[PATH_MAX];
  char interval_path[PATH_MAX];
  char target_cache_dir[PATH_MAX];
  char target_cache_path[PATH_MAX];
  char build_id[max_build_id_bytes * 2 + 1];

  /* Copy lowercase hex representation of build_id into buf.  */
  if ((build_id_len >= max_build_id_bytes) ||
      (build_id_len == 0 &&
       strlen((const char*) build_id_bytes) >= max_build_id_bytes*2))
    return -EINVAL;
  if (build_id_len == 0) /* expect clean hexadecimal */
    strcpy (build_id, (const char *) build_id_bytes);
  else
    for (int i = 0; i < build_id_len; i++)
      sprintf(build_id + (i * 2), "%02x", build_id_bytes[i]);

  urls_envvar = getenv(server_urls_envvar);
  if (urls_envvar == NULL)
    return -ENOENT;

  /* make a copy of the envvar so it can be safely modified.  */
  server_urls = malloc(strlen(urls_envvar) + 1);
  if (server_urls == NULL)
    return -ENOMEM;

  strcpy(server_urls, urls_envvar);

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
    return -ENETUNREACH;

  CURL *session = curl_easy_init();
  if (session == NULL)
    {
      curl_global_cleanup();
      return -ENETUNREACH;
    }

  /* set paths needed to perform the query

     example format
     cache_path:        $HOME/.dbgserver_cache
     target_cache_dir:  $HOME/.dbgserver_cache/0123abcd
     target_cache_path: $HOME/.dbgserver_cache/0123abcd/debuginfo  */
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
  strncpy(target_cache_dir, cache_path, PATH_MAX);
  strncat(target_cache_dir, "/", PATH_MAX - strlen(target_cache_dir));
  strncat(target_cache_dir, build_id, PATH_MAX - strlen(target_cache_dir));

  strncpy(target_cache_path, target_cache_dir, PATH_MAX);
  strncat(target_cache_path, "/", PATH_MAX - strlen(target_cache_dir));
  strncat(target_cache_path, type, PATH_MAX - strlen(target_cache_dir));

  strncpy(interval_path, cache_path, PATH_MAX);
  strncat(interval_path, "/", PATH_MAX - strlen(interval_path));
  strncat(interval_path,
          cache_clean_interval_filename,
          PATH_MAX - strlen(interval_path));

  int rc = dbgclient_init_cache(cache_path, interval_path);
  if (rc != DBGCLIENT_OK)
    return rc;

  rc = dbgclient_clean_cache(cache_path, interval_path);
  if (rc != DBGCLIENT_OK)
    return rc;

  /* If the target is already in the cache then we are done.  */
  int fd = dbgclient_get_file_from_cache(target_cache_path);
  if (fd >= 0)
    return fd;

  fd = dbgclient_add_file_to_cache(target_cache_dir,
                                   target_cache_path);
  if (fd < 0)
    /* Encountered an error adding file to cache, return error code.  */
    return fd;

  long timeout = 5; /* XXX do not hardcode.  */
  bool success = false;
  char *server_url = strtok(server_urls, url_delim);
  while (! success && server_url != NULL)
    {
      /* query servers until we find the target or run out of urls to try.  */
      long resp_code;
      char *url = build_url(server_url, build_id, type, filename);

      if (url == NULL)
        {
          close(fd);
          return -ENOMEM;
        }

      curl_easy_setopt(session, CURLOPT_URL, url);
      curl_easy_setopt(session,
                       CURLOPT_WRITEFUNCTION,
                       dbgclient_write_callback);
      curl_easy_setopt(session, CURLOPT_WRITEDATA, (void*)&fd);
      curl_easy_setopt(session, CURLOPT_TIMEOUT, timeout);

      CURLcode curl_res = curl_easy_perform(session);
      curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &resp_code);

      if (curl_res == CURLE_OK)
        switch (resp_code)
        {
        case 200:
          success = true;
          break;
        default:
          ;
        }

      free(url);
      server_url = strtok(NULL, url_delim);
    }

  free(server_urls);
  curl_easy_cleanup(session);
  curl_global_cleanup();

  if (! success)
    {
      close(fd);
      remove(target_cache_path);

      /* If target_cache_dir is empty, remove it.  */
      DIR *d = opendir(target_cache_dir);
      (void) readdir(d);
      (void) readdir(d);
      if (readdir(d) == NULL)
        remove(target_cache_dir);
      closedir(d);
      return -ENOENT;
    }

  return fd;
}

/* See dbgserver-client.h  */
int
dbgclient_find_debuginfo (const unsigned char *build_id_bytes, int build_id_len)
{
  return dbgclient_query_server(build_id_bytes, build_id_len,
                                "debuginfo", NULL);
}


/* See dbgserver-client.h  */
int
dbgclient_find_executable(const unsigned char *build_id_bytes, int build_id_len)
{
  return dbgclient_query_server(build_id_bytes, build_id_len,
                                "executable", NULL);
}

/* See dbgserver-client.h  */
int dbgclient_find_source(const unsigned char *build_id_bytes,
                          int build_id_len,
                          const char *filename)
{
  return dbgclient_query_server(build_id_bytes, build_id_len,
                                "source-file", filename);
}
