#! /bin/sh
# Copyright (C) 2011 Red Hat, Inc.
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

. $srcdir/../tests/test-subr.sh

srcdir=$srcdir/tests

testfiles tests/hello.bad-2

../../src/readelf -winfo ../tests/test-all-dies-it | grep '^ \[ *[0-9a-f]*\]' |
  sed 's/ \[ *\([0-9a-f]\+\).*/0x\1/' |
  testrun_compare ../tests/test-all-dies-it ../tests/test-all-dies-it

testrun_compare ../tests/test-all-dies-it hello.bad-2 <<EOF
0xb
EOF
