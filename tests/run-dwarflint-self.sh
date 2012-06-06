#! /bin/sh
# Copyright (C) 2009, 2010 Red Hat, Inc.
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

. $srcdir/test-subr.sh

status=0
runtest()
{
  for file; do
    if [ -f $file ]; then
      testrun ../dwarflint/dwarflint -q -i --check=@low $file ||
      { echo "*** failure in $file"; status=1; }
    fi
  done
}

runtest ../src/addr2line
runtest ../src/dwarfcmp
runtest ../src/elfcmp
runtest ../src/elflint
runtest ../src/findtextrel
runtest ../src/ld
runtest ../src/nm
runtest ../src/objdump
runtest ../src/readelf
runtest ../src/size
runtest ../src/strip
runtest ../src/unstrip
runtest ../*/*.so
runtest ../dwarflint/dwarflint

exit $status
