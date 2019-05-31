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

testfiles testfile-dbgserver.debug

if [ -z $DEBUGINFO_SERVER_LOCAL ]; then
  echo "unknown local server directory"
  exit 77
fi

if [ -z "$DEBUGINFO_SERVER" ]; then
  echo "unknown server url"
  exit 77
fi

if [ ! -w "${DEBUGINFO_SERVER_LOCAL}/buildid/" ]; then
  echo "unable to modify local server directory"
  exit 77
fi

# Build-id of testfile-dbgserver.debug
BUILD_ID="0a0cd15e690a378ec77359bb2eeb76ea0f8d67f8"
TEST_DIR="${DEBUGINFO_SERVER_LOCAL}/buildid/${BUILD_ID}"
TEST_FILE="${TEST_DIR}/${BUILD_ID}.debug"

if [ -f $TEST_DIR ]; then
  echo "Test files already exists"
  exit 77
fi

mkdir $TEST_DIR
cp testfile-dbgserver.debug $TEST_FILE

# Test whether the server is able to fetch the file from
# the local server directory.
testrun ${abs_builddir}/dbgserver-fetch -e testfile-dbgserver.debug

rm -rf $TEST_DIR

exit 0
