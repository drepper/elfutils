#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <curl/curl.h>

#define MAX_BUILD_ID_BYTES 64

#define _(Str) "dbgserver-client: "Str"\n"

const char *url_delim = " ";
const char *server_urls_envvar = "DEBUGINFO_SERVER";
const char *tmp_filename = "dbgserver_anon";

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
query_server (char *target)
{
  int fd = -1;
  long resp_code = -1;
  char *envvar;
  char *server_url;
  char *server_urls;
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
    }

  fd = syscall(__NR_memfd_create, tmp_filename, 0);

  server_url = strtok(server_urls, url_delim);
  while (server_url != NULL)
    {
      char *url = malloc(strlen(target) + strlen(server_url) + 1);

      if (server_url == NULL)
        {
          fprintf(stderr, _("out of memory"));
          return -1;
        }

      sprintf(url, "%s/%s", server_url, target);

      curl_easy_setopt(session, CURLOPT_URL, url);
      curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(session, CURLOPT_WRITEDATA, (void*)&fd);
      curl_res = curl_easy_perform(session);
      curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &resp_code);

      free(url);
      if (curl_res == CURLE_OK && resp_code == 200)
        break;

      server_url = strtok(NULL, url_delim);
    }

  free(server_urls);
  curl_easy_cleanup(session);
  curl_global_cleanup();

  if (resp_code != 200)
    {
      close(fd);
      return -1;
    }

  return fd;
}


int
dbgserver_find_elf (const unsigned char *build_id, int build_id_len)
{
  int fd ;
  int url_len;
  char *url;
  char id_buf[MAX_BUILD_ID_BYTES + 1];

  /* copy hex representation of buildid into id_buf.  */
  for (int i = 0; i < build_id_len; i++)
    sprintf(id_buf + (i * 2), "%02x", build_id[i]);

  /* url format: $DEBUGINFO_SERVER/buildid/HEXCODE/debuginfo  */
  url_len = strlen("buildid/") + build_id_len * 2 + strlen("/executable") + 1;

  url = (char*)malloc(url_len);
  if (url == NULL)
    {
      fprintf(stderr, _("out of memory"));
      return -1;
    }

  sprintf(url, "buildid/%s/executable", id_buf);
  fd = query_server(url);
  free(url);

  return fd;
}

int
dbgserver_find_debuginfo (const unsigned char *build_id, int build_id_len)
{
  int fd ;
  int url_len;
  char *url;
  char id_buf[MAX_BUILD_ID_BYTES + 1];

  /* copy hex representation of buildid into id_buf.  */
  for (int i = 0; i < build_id_len; i++)
    sprintf(id_buf + (i * 2), "%02x", build_id[i]);

  /* url format: $DEBUGINFO_SERVER/buildid/HEXCODE/debuginfo  */
  url_len = strlen("buildid/") + build_id_len * 2 + strlen("/debuginfo") + 1;

  url = (char*)malloc(url_len);
  if (url == NULL)
    {
      fprintf(stderr, _("out of memory"));
      return -1;
    }

  sprintf(url, "buildid/%s/debuginfo", id_buf);
  fd = query_server(url);
  free(url);

  return fd;
}
