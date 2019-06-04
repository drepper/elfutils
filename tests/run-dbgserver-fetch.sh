# Copyright (C) 2019 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh


if [ -z "$DEBUGINFO_SERVER" ]; then
  echo "unknown server url"
  exit 77
fi

testfiles testfile-dbgserver.debug

# init db if needed and add testfile entry
DB=${HOME}/.dbgserver.sqlite
BUILD_ID="0a0cd15e690a378ec77359bb2eeb76ea0f8d67f8"
ARTIFACT="D"
MTIME=`stat -c %Y testfile-dbgserver.debug`
SOURCETYPE="F"
SOURCE_0=`realpath testfile-dbgserver.debug`

sqlite3 $DB << EOF
create table if not exists
    buildids (
        buildid text not null,                          -- the buildid
        artifacttype text(1) not null
            check (artifacttype IN ('D', 'S', 'E')),    -- d(ebug) or s(sources) or e(xecutable)
        mtime integer not null,                         -- epoch timestamp when we last found this
        sourcetype text(1) not null
            check (sourcetype IN ('F', 'R', 'R', 'L')), -- as per --source-TYPE single-char code
        source0 text,                                   -- more sourcetype-specific location data
        source1 text);                                  -- more sourcetype-specific location data
create index if not exists buildids_idx1 on buildids (buildid, artifacttype);
create unique index if not exists buildids_idx2 on buildids (buildid, artifacttype, sourcetype, source0, source1);
insert into buildids values ('${BUILD_ID}', '${ARTIFACT}', ${MTIME}, '${SOURCETYPE}', '${SOURCE_0}', NULL)
EOF

../../src/dbgserver -vv &
PID=$!
sleep 2

# Test whether the server is able to fetch the file from the local dbgserver.
testrun ${abs_builddir}/dbgserver-fetch -e testfile-dbgserver.debug

kill $PID
sqlite3 $DB << EOF
delete from buildids where buildid='${BUILD_ID}' AND artifacttype='${ARTIFACT}'
    AND mtime=${MTIME} and sourcetype='${SOURCETYPE}' and source0='${SOURCE_0}'
EOF

exit 0
