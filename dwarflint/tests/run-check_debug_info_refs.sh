#! /bin/sh
# Copyright (C) 2010, 2011 Red Hat, Inc.
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

testfiles check_debug_info_refs-{1,2}

testrun_compare ./dwarflint --check=check_debug_info_refs check_debug_info_refs-1 <<EOF
error: .debug_aranges: table 48 (CU DIE 95): there has already been arange section for this CU.
EOF

testrun_compare ./dwarflint --strict --check=check_debug_info_refs check_debug_info_refs-1 <<EOF
warning: .debug_info: DIE 0xb (abbreviation 0): DIE chain not terminated with null entry.
warning: .debug_info: DIE 0x5f (abbreviation 54): DIE chain not terminated with null entry.
error: .debug_aranges: table 48 (CU DIE 95): there has already been arange section for this CU.
warning: .debug_info: CU 0: no aranges table is associated with this CU.
EOF

testrun_compare ./dwarflint --strict --check=check_debug_info_refs check_debug_info_refs-2 <<EOF
warning: .debug_info: DIE 0xb (abbreviation 0): DIE chain not terminated with null entry.
warning: .debug_info: DIE 0x54 (abbreviation 48): DIE chain not terminated with null entry.
warning: .debug_line: table 0: empty line number program.
error: .debug_line: table 0: sequence of opcodes not terminated with DW_LNE_end_sequence.
warning: .debug_info: CU 0: no aranges table is associated with this CU.
EOF
