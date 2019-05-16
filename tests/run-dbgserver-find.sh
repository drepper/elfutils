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

testfiles testfile-dbgserver.exec
testfiles testfile-dbgserver.debug

EXPECT_FAIL=0
EXPECT_PASS=1
DB=${PWD}/.dbgserver_tmp.sqlite
export DBGSERVER_CACHE_PATH=${PWD}/.client_cache

# find an unused port number
while true; do
    PORT=`expr '(' $RANDOM % 1000 ')' + 9000`
    ss -atn | fgrep ":$PORT" || break
done    

../../dbgserver/dbgserver -vvv -d $DB -F $PWD -p $PORT &
PID=$!
trap 'kill $PID || true; rm -f $DB' 0 1 5 9 15
sleep 5

export DBGSERVER_URLS=http://localhost:$PORT # XXX: no / at end; dbgserver rejects extra /

# Test whether the server is able to fetch the file from the local dbgserver.
testrun ${abs_builddir}/dbgserver_build_id_find -e testfile-dbgserver.exec $EXPECT_PASS

kill $PID
rm $DB

# Run the test again without the server running. The target file should
# be found in the cache.
testrun ${abs_builddir}/dbgserver_build_id_find -e testfile-dbgserver.exec $EXPECT_PASS

# Trigger a cache clean and run the test again. The client should be unable to
# find the target.
echo 0 > $DBGSERVER_CACHE_PATH/cache_clean_interval_s
testrun ${abs_builddir}/dbgserver_build_id_find -e testfile-dbgserver.exec $EXPECT_FAIL

rm -rf $DBGSERVER_CACHE_PATH
exit 0
