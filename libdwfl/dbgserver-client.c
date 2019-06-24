#include "dbgserver-client.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define MAX_BUILD_ID_BYTES 64
#define _(Str) "dbgserver-client: "Str"\n"

const char *url_delim = " ";
const char *server_urls_envvar = "DBGSERVER_URLS";
const char *cache_path_envvar = "DBGSERVER_CACHE_PATH";
const char *cache_clean_interval_envvar = "DBGSERVER_CACHE_CLEAN_INTERVAL";
const char *cache_default_name = ".dbgserver_client_cache";

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

void
clean_cache(char *cache_path)
{
  (void) cache_path;
  return;
}

int
query_server (char *build_id, char *type)
{
  int fd = -1;
  bool success = false;
  long resp_code = -1;
  long timeout = 5;
  char *envvar;
  char *server_url;
  char *server_urls;
  char cache_path[PATH_MAX];
  char target_cache_dir[PATH_MAX];
  char target_cache_path[PATH_MAX];
  CURL *session;
  CURLcode curl_res;

  envvar = getenv(server_urls_envvar);
  if (envvar == NULL)
    {
      fprintf(stderr, _("cannot find server urls environment variable"));
      return -1;
    }

  server_urls = malloc(strlen(envvar) + 1);
  strcpy(server_urls, envvar);
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

  session = curl_easy_init();
  if (session == NULL)
    {
      fprintf(stderr, _("unable to begin curl session"));
      curl_global_cleanup();
      return -1;
    }

  /* set paths needed to perform query

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

  fd = get_file_from_cache(target_cache_path);
  if (fd >= 0)
    return fd;

  fd = add_file_to_cache(cache_path, target_cache_dir, target_cache_path);
  if (fd < 0)
    /* encountered an error adding file to cache.  */
    return -1;

  /* query servers until we find the target or run out of urls to try.  */
  server_url = strtok(server_urls, url_delim);
  while (! success && server_url != NULL)
    {
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
      curl_res = curl_easy_perform(session);
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
      return -1;
    }

  clean_cache(cache_path);
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
