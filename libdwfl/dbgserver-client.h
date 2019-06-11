int dbgserver_enabled(void);
int dbgserver_find_debuginfo(const unsigned char *build_id,
                             int build_id_len);
int dbgserver_find_elf(const unsigned char *build_id,
                       int build_id_len);
