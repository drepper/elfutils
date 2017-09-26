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
