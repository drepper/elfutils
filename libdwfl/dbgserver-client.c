#include "dbgserver-client.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <string.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define MAX_BUILD_ID_BYTES 64
#define _(Str) "dbgserver-client: "Str"\n"

/* URLs of dbgservers, separated by url_delim.
   This env var must be set for dbgserver-client to run.  */
const char *server_urls_envvar = "DBGSERVER_URLS";
const char *url_delim = " ";

/* Location of the cache of files downloaded from dbgservers.
   The default parent directory is $HOME, or '/' if $HOME doesn't exist.  */
const char *cache_path_envvar = "DBGSERVER_CACHE_PATH";
const char *cache_default_name = ".dbgserver_client_cache";

/* Files will be removed from the cache when the number of seconds
   since their st_mtime is greater than or equal to this env var.  */
const char *cache_clean_interval_envvar = "DBGSERVER_CACHE_CLEAN_INTERVAL_S";
const unsigned long cache_clean_interval_default = 86400; /* 1 day  */

int
dbgserver_enabled (void)
{
  return getenv(server_urls_envvar) != NULL;
}

size_t
write_callback (char *ptr, size_t size, size_t nmemb, void *fdptr)
{
  int fd = *(int*)fdptr;
  ssize_t res;
  ssize_t count = size * nmemb;

  res = write(fd, (void*)ptr, count);
  if (res < 0)
    return (size_t)0;

  return (size_t)res;
}

int
get_file_from_cache (char *target_cache_path)
{
  int fd;
  struct stat st;

  if (stat(target_cache_path, &st) == -1)
    return -1;

  fd = open(target_cache_path, O_RDONLY);
  if (fd < 0)
    fprintf(stderr, _("error opening target from cache"));

  return fd;
}

int
add_file_to_cache (char *cache_path,
                   char *target_cache_dir,
                   char *target_cache_path)
{
  int fd;
  struct stat st;

  /* create cache if not found.  */
  if (stat(cache_path, &st) == -1 && mkdir(cache_path, 0777) < 0)
    fprintf(stderr, _("error finding cache"));

  /* create target directory in cache if not found.  */
  if (stat(target_cache_dir, &st) == -1 && mkdir(target_cache_dir, 0777) < 0)
    fprintf(stderr, _("error finding target cache directory"));

  /* create target file if not found.  */
  fd = open(target_cache_path, O_CREAT | O_RDWR, 0666);
  if (fd < 0)
    fprintf(stderr, _("error finding target in cache"));

  return fd;
}

/* Delete any files that have been unmodied for a period
   longer than $DBGSERVER_CACHE_CLEAN_INTERVAL_S.  */
void
clean_cache(char *cache_path)
{
  char * const dirs[] = { cache_path, NULL, };

  FTS *fts = fts_open(dirs, 0, NULL);
  if (fts == NULL)
    {
      if (errno == ENOENT)
        {
          errno = 0;
          return;
        }

      fprintf(stderr, _("error cleaning cache, cannot fts_open"));
      return;
    }

  time_t clean_interval;
  const char *interval_str = getenv(cache_clean_interval_envvar);
  if (interval_str == NULL
      || sscanf(interval_str, "%ld", &clean_interval) != 1)
    clean_interval = cache_clean_interval_default;

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
}

int
query_server (char *build_id, char *type)
{
  char *urls_envvar;
  char *server_urls;
  char cache_path[PATH_MAX];
  char target_cache_dir[PATH_MAX];
  char target_cache_path[PATH_MAX];

  urls_envvar = getenv(server_urls_envvar);
  if (urls_envvar == NULL)
    {
      fprintf(stderr, _("cannot find server urls environment variable"));
      return -1;
    }

  /* make a copy of the envvar so it can be safely modified.  */
  server_urls = malloc(strlen(urls_envvar) + 1);
  strcpy(server_urls, urls_envvar);
  if (server_urls == NULL)
    {
      fprintf(stderr, _("out of memory"));
      return -1;
    }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
    {
      fprintf(stderr, _("unable to initialize curl"));
      return -1;
    }

  CURL *session = curl_easy_init();
  if (session == NULL)
    {
      fprintf(stderr, _("unable to begin curl session"));
      curl_global_cleanup();
      return -1;
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

  clean_cache(cache_path);

  int fd = get_file_from_cache(target_cache_path);
  if (fd >= 0)
    return fd;

  fd = add_file_to_cache(cache_path, target_cache_dir, target_cache_path);
  if (fd < 0)
    /* encountered an error adding file to cache.  */
    return -1;

  long timeout = 5; /* XXX grab from env var.  */
  bool success = false;
  char *server_url = strtok(server_urls, url_delim);
  while (! success && server_url != NULL)
    {
      /* query servers until we find the target or run out of urls to try.  */
      long resp_code;
      char *url = malloc(strlen(server_url)
                  + strlen("/buildid//")
                  + strlen(build_id)
                  + strlen(type)
                  + 1);

      if (server_url == NULL)
        {
          fprintf(stderr, _("out of memory"));
          close(fd);
          return -1;
        }

      sprintf(url, "%s/buildid/%s/%s", server_url, build_id, type);

      curl_easy_setopt(session, CURLOPT_URL, url);
      curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, write_callback);
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
      /* remove any unnecessary directories that were just created.  */
      clean_cache(cache_path);
      return -1;
    }

  return fd;
}

int
dbgserver_build_id_find (enum dbgserver_file_type file_type,
                         const unsigned char *build_id,
                         int build_id_len)
{
  char *type;
  char id_buf[MAX_BUILD_ID_BYTES + 1];

  /* copy hex representation of buildid into id_buf.  */
  for (int i = 0; i < build_id_len; i++)
    sprintf(id_buf + (i * 2), "%02x", build_id[i]);

  switch (file_type)
  {
  case dbgserver_file_type_debuginfo:
    type = "debuginfo";
    break;
  case dbgserver_file_type_executable:
    type = "executable";
    break;
  case dbgserver_file_type_source:
    type = "source";
    break;
  default:
    assert(0);
  }

  return query_server(id_buf, type);
}
