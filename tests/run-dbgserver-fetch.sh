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

if [ -f $DB ]; then
  rm $DB
fi  

# init the DB
../../src/dbgserver -vv &
PID=$!
sleep 5
kill $PID

sqlite3 $DB << EOF
insert into buildids values (
    '${BUILD_ID}', '${ARTIFACT}', ${MTIME}, '${SOURCETYPE}', '${SOURCE_0}', NULL)
EOF

../../src/dbgserver -vv &
PID=$!
sleep 5

# Test whether the server is able to fetch the file from the local dbgserver.
testrun ${abs_builddir}/dbgserver-fetch -e testfile-dbgserver.debug

kill $PID

exit 0
