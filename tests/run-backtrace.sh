#! /bin/sh
# Copyright (C) 2012 Red Hat, Inc.
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

if [ -z "$VERBOSE" ]; then
  exec >/dev/null
fi

mytestrun()
{
  echo "$*"
  testrun "$@"
}

check_gsignal()
{
  # Without proper ELF symbols resolution we could get inappropriate weak
  # symbol "gsignal" with the same address as the correct symbol "raise".
  if grep -w gsignal $core.bt; then
    false
  fi
}

for child in backtrace-child{,-biarch}; do
  mytestrun ./backtrace ./$child
  core="core.`ulimit -c unlimited; set +e; ./$child --gencore --run; true`"
  tempfiles $core{,.bt}
  mytestrun ./backtrace ./$child ./$core | tee $core.bt
  check_gsignal $core.bt
done

for arch in ppc ppc64 s390 s390x; do
  testfiles backtrace.$arch.{exec,core}
  tempfiles backtrace.$arch.bt
  echo ./backtrace ./backtrace.$arch.{exec,core}
  mytestrun ./backtrace ./backtrace.$arch.{exec,core} | tee backtrace.$arch.bt
  check_gsignal backtrace.$arch.bt
done

exit 0
