enum dbgserver_file_type {
  dbgserver_file_type_debuginfo,
  dbgserver_file_type_executable,
  dbgserver_file_type_source,
};

/* Returns 1 if $DEBUGINFO_SERVER is defined, otherwise 0.  */
int dbgserver_enabled (void);

/* Query the urls contained in $DEBUGINFO_SERVER for a file with
   the specified type and build id.  */
int dbgserver_build_id_find (enum dbgserver_file_type type,
                             const unsigned char *build_id,
                             int build_id_len);
