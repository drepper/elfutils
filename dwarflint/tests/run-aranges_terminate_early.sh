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

# Hand-crafted file that has 0,0 pair in aranges presented before the
# actual end of the table.
testfiles tests/aranges_terminate_early

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --strict aranges_terminate_early <<EOF
warning: .debug_aranges: [0x20, 0x30): unnecessary padding with zero bytes.
warning: .debug_aranges: addresses [0x400474, 0x400481) are covered with CU DIEs, but not with aranges.
EOF

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=check_debug_aranges --strict aranges_terminate_early <<EOF
warning: .debug_aranges: [0x20, 0x30): unnecessary padding with zero bytes.
warning: .debug_aranges: addresses [0x400474, 0x400481) are covered with CU DIEs, but not with aranges.
EOF
