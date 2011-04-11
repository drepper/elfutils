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

# Following program compiled with "default" gcc settings,
# which is dwarf-2 + gnu extensions. Which will result in:
#
#  [    a2]      subrange_type
#                type                 (ref4) [    ac]
#                upper_bound          (block1)
#                [   0] fbreg -24
#                [   2] deref
#
# According to dwarf-2 DW_AT_upperbound cannot be encoded with block form.
# It can however with later versions of dwarf, which gcc will output as
# gnu extension (unless -gstrict-dwarf is given).
#
# int max_range = 42;
#
# int main (int argc, char **argv)
# {
#   char chars[max_range];
#   chars[max_range -1] = 7;
#   return 0;
# }
#
# This would crash the low-level check_debug_info in the past.
testfiles upper

testrun_compare ./dwarflint --quiet --check=@low upper <<EOF
EOF
