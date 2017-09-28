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

testfiles tests/DW_AT-later-version

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint --nognu DW_AT-later-version <<EOF
warning: .debug_abbrev: abbr. 0x11, attr. endianity: attribute from later DWARF version.
warning: .debug_info: DIE 0xb: DW_AT_low_pc value not below DW_AT_high_pc.
warning: .debug_info: DIE 0x29: variable has decl_file, but NOT decl_line
EOF
