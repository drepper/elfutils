#! /bin/sh
# Copyright (C) 2011 Red Hat, Inc.
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

testfiles hello.bad-1 hello.bad-3

testrun_compare ./dwarflint hello.bad-1 <<EOF
error: .debug_info: DIE 0x83: abbrev section at 0x0 doesn't contain code 83.
EOF

testrun_compare ./dwarflint hello.bad-3 <<EOF
error: .debug_info: DIE 0x91: toplevel DIE chain contains more than one DIE.
error: .debug_info: DIE 0x98: toplevel DIE chain contains more than one DIE.
error: .debug_info: DIE 0x9e: toplevel DIE chain contains more than one DIE.
error: .debug_info: DIE 0xa4: toplevel DIE chain contains more than one DIE.
error: .debug_info: DIE 0xab: toplevel DIE chain contains more than one DIE.
EOF
