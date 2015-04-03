#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

testfiles testfile39 testfile-debug-types

testrun_compare ${abs_top_builddir}/tests/test-iterators testfile39 <<\EOF
0xb
0x9e
0x135
0x1c8
0 7
0 7
0 7
0 7
EOF

testrun_compare ${abs_top_builddir}/tests/test-iterators testfile-debug-types <<\EOF
0xb
0x17
0x5a
4 6
0 9
0 3
0 6
0 6
2 3
1 4
0 2
0 5
1 3
2 4
0 3
0 5
EOF

exit 0
