#ifdef __cplusplus
extern "C" {
#endif

/* Query the urls contained in $DBGSERVER_URLS for a file with
   the specified type and build id.  If build_id_len == 0, the
   build_id is supplied as a lowercase hexadecimal string; otherwise
   it is a binary blob of given legnth.

   If successful, return a file descriptor to the target, otherwise
   return a posix error code. */
int dbgclient_find_debuginfo (const unsigned char *build_id_bytes,
                             int build_id_len,
                             char **path);

int dbgclient_find_executable (const unsigned char *build_id_bytes,
                               int build_id_len,
                               char **path);

int dbgclient_find_source (const unsigned char *build_id_bytes,
                           int build_id_len,
                           const char *filename,
                           char **path);

#ifdef __cplusplus
}
#endif
