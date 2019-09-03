/* External declarations for the libdbgserver client library.
   Copyright (C) 2019 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

   * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

   * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _LIBBGSERVER_CLIENT_H
#define _LIBBGSERVER_CLIENT_H 1

/* Names of environment variables that control the client logic. */
#define DBGSERVER_URLS_ENV_VAR "DBGSERVER_URLS"
#define DBGSERVER_CACHE_PATH_ENV_VAR "DBGSERVER_CACHE_PATH"
#define DBGSERVER_TIMEOUT_ENV_VAR "DBGSERVER_TIMEOUT"

#ifdef __cplusplus
extern "C" {
#endif

/* Query the urls contained in $DBGSERVER_URLS for a file with
   the specified type and build id.  If build_id_len == 0, the
   build_id is supplied as a lowercase hexadecimal string; otherwise
   it is a binary blob of given legnth.

   If successful, return a file descriptor to the target, otherwise
   return a posix error code.  If successful, set *path to a
   strdup'd copy of the name of the same file in the cache.
   Caller must free() it later. */
  
int dbgserver_find_debuginfo (const unsigned char *build_id,
                             int build_id_len,
                             char **path);

int dbgserver_find_executable (const unsigned char *build_id,
                               int build_id_len,
                               char **path);

int dbgserver_find_source (const unsigned char *build_id,
                           int build_id_len,
                           const char *filename,
                           char **path);

#ifdef __cplusplus
}
#endif


#endif /* _LIBBGSERVER_CLIENT_H */
