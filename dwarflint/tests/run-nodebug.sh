#! /bin/sh
# Copyright (C) 2010, 2011 Red Hat, Inc.
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

testfiles tests/nodebug tests/null.o

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint nodebug <<EOF
error: .debug_abbrev: data not found.
error: .debug_info: data not found.
EOF

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint -i nodebug <<EOF
No errors
EOF

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint -q -i nodebug <<EOF
EOF

# This has nothing to do with the nodebug test, but we can just as
# well stick it in there.
testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=oentuh -q nodebug <<EOF
warning: the rule \`oentuh' never matched.
EOF

# ... and since we are testing this here, also check that we don't get
# this message in situations where it makes no sense.
LANG=C testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=oentuh -q noeuht <<EOF
error: Cannot open input file: No such file or directory.
EOF

LANG=C testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=oentuh -q noeuht nodebug <<EOF

noeuht:
error: Cannot open input file: No such file or directory.

nodebug:
warning: the rule \`oentuh' never matched.
EOF

LANG=C testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=oentuh -q nodebug nodebug <<EOF

nodebug:

nodebug:
warning: the rule \`oentuh' never matched.
EOF

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint null.o <<EOF
warning: .debug_abbrev: [0x0, 0x1): unnecessary padding with zero bytes.
warning: .debug_abbrev: no abbreviations.
error: .debug_info: data not found.
EOF

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --ignore-bloat --nodebug:ignore null.o <<EOF
No errors
EOF
