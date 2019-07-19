/* Query the urls contained in $DBGSERVER_URLS for a file with
   the specified type and build id. If successful, return
   a file descriptor to the target, otherwise return an
   error code */
int dbgclient_find_debuginfo (const unsigned char *build_id_bytes,
                             int build_id_len);

int dbgclient_find_executable (const unsigned char *build_id_bytes,
                               int build_id_len);

int dbgclient_find_source (const unsigned char *build_id_bytes,
                           int build_id_len,
                           char *filename);
