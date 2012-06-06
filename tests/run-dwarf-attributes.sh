#! /bin/sh
# Copyright (C) 2009 Red Hat, Inc.
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

testfiles testfile

testrun_compare ./dwarf-print --offsets --depth=1 testfile <<\EOF
testfile:
 <compile_unit offset=[0xb] stmt_list=[{5 line entries}] high_pc=0x804845a low_pc=0x804842c name="m.c" comp_dir="/home/drepper/gnu/new-bu/build/ttt" producer="GNU C 2.96 20000731 (Red Hat Linux 7.0)" language=C89>...
 <compile_unit offset=[0xca] stmt_list=[{4 line entries}] high_pc=0x8048466 low_pc=0x804845c name="b.c" comp_dir="/home/drepper/gnu/new-bu/build/ttt" producer="GNU C 2.96 20000731 (Red Hat Linux 7.0)" language=C89>...
 <compile_unit offset=[0x15fc] stmt_list=[{4 line entries}] high_pc=0x8048472 low_pc=0x8048468 name="f.c" comp_dir="/home/drepper/gnu/new-bu/build/ttt" producer="GNU C 2.96 20000731 (Red Hat Linux 7.0)" language=C89>...
EOF

exit 0
