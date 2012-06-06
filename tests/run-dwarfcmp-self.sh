#! /bin/sh
# Copyright (C) 2009 Red Hat, Inc.
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

dwarfcmp=${dwarfcmp:-../src/dwarfcmp}
try_switches=${try_switches:- -iq -i -il}

status=0
run_one()
{
  file="$1"; shift
  testrun "$dwarfcmp" "$@" "$file" "$file" ||
  { echo "*** failure in $dwarfcmp $* on $file"; status=1; }
}

runtest()
{
  for file; do
    if [ -f $file ]; then
      for try in $try_switches; do
        run_one "$file" $try
      done
    fi
  done
}

runtest ../src/addr2line
runtest ../src/ar
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

# These are the biggest ones.
runtest ../dwarflint/dwarflint
runtest ../src/dwarfcmp
runtest ../src/dwarfcmp-test

exit $status
