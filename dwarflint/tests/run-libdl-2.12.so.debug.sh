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

testfiles tests/libdl-2.12.so.debug

# Here we test that dwarflint can tolerate invalid attribute name.
testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=@low --nognu --ignore-bloat libdl-2.12.so.debug <<EOF
warning: .debug_abbrev: abbr. attribute 0xbe: invalid or unknown name 0x2107.
warning: .debug_abbrev: abbr. attribute 0x330: invalid or unknown name 0x2107.
warning: .debug_abbrev: abbr. attribute 0xa28: invalid or unknown name 0x2107.
warning: .debug_abbrev: abbr. attribute 0x108e: invalid or unknown name 0x2107.
warning: .debug_abbrev: abbr. attribute 0x1300: invalid or unknown name 0x2107.
warning: .debug_info: DIE 0xd9a8: DW_AT_low_pc value not below DW_AT_high_pc.
warning: .debug_info: DIE 0xdcd7: DW_AT_low_pc value not below DW_AT_high_pc.
warning: .debug_info: CU 55709: no aranges table is associated with this CU.
warning: .debug_info: CU 56524: no aranges table is associated with this CU.
EOF

# Here we test proper support for DW_AT_GNU_vector
testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --check=@low --ignore-bloat libdl-2.12.so.debug <<EOF
warning: .debug_info: DIE 0xd9a8: DW_AT_low_pc value not below DW_AT_high_pc.
warning: .debug_info: DIE 0xdcd7: DW_AT_low_pc value not below DW_AT_high_pc.
warning: .debug_info: CU 55709: no aranges table is associated with this CU.
warning: .debug_info: CU 56524: no aranges table is associated with this CU.
EOF
