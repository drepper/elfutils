. $srcdir/test-subr.sh

testfiles testfile-dbgserver.debug

orig_url=$DEBUGINFO_SERVER
export DEBUGINFO_SERVER="${DEBUGINFO_SERVER}/test/"

testrun ${abs_builddir}/dbgserver-fetch -e testfile-dbgserver.debug
export DEBUGINFO_SERVER=$orig_url

exit 0
