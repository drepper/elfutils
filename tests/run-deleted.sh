#! /bin/bash
# Copyright (C) 2014 Red Hat, Inc.
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

# Older Linux (such as 2.6.32) required PTRACE_ATTACH to read /proc/PID/mem.
sleep 60 & p=$!; sleep 0.1
addr=0x$(cat /proc/$p/maps|sed -n 's#^\([0-9a-f]*\)-[0-9a-f]* r[^ ]* 00* .*/sleep$#\1#p'|head -n1)
supported=$[$(dd if=/proc/$p/mem bs=1 skip=$[$addr] count=1|wc -c)]
kill -9 $p
if [ $supported -eq 0 ]; then
  exit 77
fi

tempfiles deleted deleted-lib.so
cp -p ${abs_builddir}/deleted ${abs_builddir}/deleted-lib.so .
pid=$(testrun ${abs_builddir}/deleted)
sleep 1
tempfiles bt
testrun ${abs_top_builddir}/src/stack -p $pid >bt
kill -9 $pid
wait
grep -w libfunc bt
grep -w main bt
