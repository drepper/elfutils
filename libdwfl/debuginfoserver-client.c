#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

int
dbgserver_enabled (void)
{
  return getenv("DEBUGINFO_SERVER") != NULL;
}

size_t
write_data (void *ptr, size_t size, size_t nmemb, FILE *file)
{
  // TODO
  (void) ptr;
  (void) size;
  (void) nmemb;
  (void) file;

  return (size_t)0;
}

int
dbgserver_find_debuginfo (const unsigned char *build_id,
                          int build_id_len)
{
  (void) build_id;
  (void) build_id_len;

  // TODO: build url using $DEBUGINFO_SERVER and build_id
  char *url = "localhost:80/buildid/test";
  CURL *curl;
  CURLcode res;

  // TODO: report errors
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
    return -1;

  curl = curl_easy_init();
  if (!curl)
    {
      curl_global_cleanup();
      return -1;
    }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  res = curl_easy_perform(curl);

  if (res == CURLE_OK)
    {
      // TODO: check size and copy into buffer
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

  return -1;
}
