#! /bin/sh
# Copyright (C) 2010 Red Hat, Inc.
# This file is part of Red Hat elfutils.
#
# Red Hat elfutils is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# Red Hat elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with Red Hat elfutils; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.
#
# Red Hat elfutils is an included package of the Open Invention Network.
# An included package of the Open Invention Network is a package for which
# Open Invention Network licensees cross-license their patents.  No patent
# license is granted, either expressly or impliedly, by designation as an
# included package.  Should you wish to participate in the Open Invention
# Network licensing program, please visit www.openinventionnetwork.com
# <http://www.openinventionnetwork.com>.

. $srcdir/../tests/test-subr.sh

srcdir=$srcdir/tests

testfiles nodebug

testrun_compare ./dwarflint nodebug <<EOF
error: .debug_abbrev: data not found.
error: .debug_info: data not found.
EOF

testrun_compare ./dwarflint -i nodebug <<EOF
No errors
EOF

testrun_compare ./dwarflint -q -i nodebug <<EOF
EOF

# This has nothing to do with the nodebug test, but we can just as
# well stick it in there.
testrun_compare ./dwarflint --check=oentuh -q nodebug <<EOF
warning: the rule \`oentuh' never matched.
EOF

# ... and since we are testing this here, also check that we don't get
# this message in situations where it makes no sense.
LANG=C testrun_compare ./dwarflint --check=oentuh -q noeuht <<EOF
error: Cannot open input file: No such file or directory.
EOF

LANG=C testrun_compare ./dwarflint --check=oentuh -q noeuht nodebug <<EOF

noeuht:
error: Cannot open input file: No such file or directory.

nodebug:
warning: the rule \`oentuh' never matched.
EOF

LANG=C testrun_compare ./dwarflint --check=oentuh -q nodebug nodebug <<EOF

nodebug:

nodebug:
warning: the rule \`oentuh' never matched.
EOF
