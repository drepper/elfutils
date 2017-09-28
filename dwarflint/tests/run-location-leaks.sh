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

testfiles tests/location-leaks

testrun_compare ${abs_top_builddir}/dwarflint/dwarflint location-leaks <<EOF
warning: .debug_loc: loclist 0x38: entry covers no range.
error: .debug_info: DIE 0x62: attribute \`location': PC range [0x400495, 0x40049a) outside containing scope.
error: .debug_info: DIE 0x51: in this context: [0x400498, 0x4004b2).
EOF
