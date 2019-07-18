/* Return codes.  */
#define DBGCLIENT_E_OK -1
/* Client is not enabled for use.  */
#define DBGCLIENT_E_NOT_ENABLED -2
/* Cannot create either a file in the cache or the cache itself.  */
#define DBGCLIENT_E_CACHE_CANT_CREATE -3
/* Cannot open either a file in the cache or the cache itself.  */
#define DBGCLIENT_E_CACHE_CANT_OPEN -4
/* Cannot read either a file in the cache or the cache itself.  */
#define DBGCLIENT_E_CACHE_CANT_READ -5
/* Cannot write either a file in the cache or the cache itself.  */
#define DBGCLIENT_E_CACHE_CANT_WRITE -6
/* Out of memory.  */
#define DBGCLIENT_E_OUT_OF_MEMORY -7
/* Client was unable to locate the target on any dbgserver.  */
#define DBGCLIENT_E_TARGET_NOT_FOUND -8
/* Early init code failed, cannot connect with any dbgserver.  */
#define DBGCLIENT_E_CANT_INIT_CONNECTION -9

/* Indicates the type of target file.  */
enum dbgclient_file_type {
  dbgclient_file_type_debuginfo,
  dbgclient_file_type_executable,
  dbgclient_file_type_source,
};

/* Returns 1 if $DBGSERVER_URLS is defined, otherwise 0.  */
int dbgclient_enabled (void);

/* Query the urls contained in $DBGSERVER_URLS for a file with
   the specified type and build id.  */
int dbgclient_build_id_find (enum dbgclient_file_type type,
                             const unsigned char *build_id,
                             int build_id_len);
