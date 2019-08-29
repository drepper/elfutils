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

EXPECT_FAIL=0
EXPECT_PASS=1
DB=${PWD}/.dbgserver_tmp.sqlite
export DBGSERVER_CACHE_PATH=${PWD}/.client_cache

# find an unused port number
while true; do
    PORT=`expr '(' $RANDOM % 1000 ')' + 9000`
    ss -atn | fgrep ":$PORT" || break
done    

# We want to run dbgserver in the background.  We also want to start
# it with the same check/installcheck-sensitive LD_LIBRARY_PATH stuff
# that the testrun alias sets.  But: we if we just use
#    testrun .../dbgserver
# it runs in a subshell, with different pid, so not helpful.
#
# So we gather the LD_LIBRARY_PATH with this cunning trick:

ldpath=`testrun sh -c 'echo $LD_LIBRARY_PATH'`

# Compile a simple program, strip its debuginfo and save the build-id.
# Also move the debuginfo into another directory so that elfutils
# cannot find it without dbgserver.
echo "int main() { return 0; }" > ${PWD}/prog.c
gcc -g -o prog ${PWD}/prog.c
 ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog
BUILD_ID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a prog | grep 'Build ID' | cut -d ' ' -f 7`
mkdir ${PWD}/debug
mv prog.debug ${PWD}/debug
tempfiles prog*

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../dbgserver/dbgserver -vvv -d $DB -F $PWD -p $PORT &
PID=$!
sleep 5
tempfiles .dbgserver_*

export DBGSERVER_URLS=http://localhost:$PORT/   # or without trailing /

# Test whether the server is able to fetch the file from the local dbgserver.
testrun ${abs_builddir}/dbgserver_build_id_find -e prog $EXPECT_PASS

# Test whether dbgserver-find is able to fetch files from the local dbgserver.
rm -rf $DBGSERVER_CACHE_PATH
testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find debuginfo $BUILD_ID <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/debuginfo
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find executable $BUILD_ID <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/executable
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find source $BUILD_ID ${PWD}/prog.c <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/$(echo source${PWD}/prog.c | sed -e 's\[/.]\#\g')
EOF


kill -INT $PID
sleep 5
tempfiles .dbgserver_*

# Run the tests again without the server running. The target file should
# be found in the cache.
testrun ${abs_builddir}/dbgserver_build_id_find -e prog $EXPECT_PASS

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find debuginfo $BUILD_ID <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/debuginfo
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find executable $BUILD_ID <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/executable
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find source $BUILD_ID ${PWD}/prog.c <<EOF
${DBGSERVER_CACHE_PATH}/${BUILD_ID}/$(echo source${PWD}/prog.c | sed -e 's\[/.]\#\g')
EOF


# Trigger a cache clean and run the tests again. The clients should be unable to
# find the target.
echo 0 > $DBGSERVER_CACHE_PATH/cache_clean_interval_s
testrun ${abs_builddir}/dbgserver_build_id_find -e prog $EXPECT_FAIL

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find debuginfo $BUILD_ID <<EOF
Server query failed: Connection refused
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find executable $BUILD_ID <<EOF
Server query failed: Connection refused
EOF

testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find source $BUILD_ID ${PWD}/prog.c <<EOF
Server query failed: Connection refused
EOF

# Ensure dbgserver-find can be safely called with no arguments
testrun_compare ${abs_top_builddir}/dbgserver/dbgserver-find <<EOF
Usage: ${abs_top_builddir}/dbgserver/dbgserver-find debuginfo BUILDID
  or:  ${abs_top_builddir}/dbgserver/dbgserver-find executable BUILDID
  or:  ${abs_top_builddir}/dbgserver/dbgserver-find source BUILDID /FILENAME
EOF

rm -rf debug
rm -rf $DBGSERVER_CACHE_PATH
exit 0
