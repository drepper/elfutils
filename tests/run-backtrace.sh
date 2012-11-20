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

check_main()
{
  if grep -w main $1; then
    return
  fi
  cat >&2 $1
  echo >&2 $2: no main
  false
}

check_gsignal()
{
  # Without proper ELF symbols resolution we could get inappropriate weak
  # symbol "gsignal" with the same address as the correct symbol "raise".
  if ! grep -w gsignal $1; then
    return
  fi
  cat >&2 $1
  echo >&2 $2: found gsignal
  false
}

check_err()
{
  if test ! -s $1; then
    return
  fi
  # In some cases we cannot reliably find out we got behind _start.
  if echo "No DWARF information found" | cmp -s - $1; then
    return
  fi
  cat >&2 $1
  echo >&2 $2: neither empty nor just out of DWARF
}

for child in backtrace-child{,-biarch}; do
  tempfiles $child{.bt,err}
  ./backtrace ./$child 1>$child.bt 2>$child.err \
    || true
  check_main $child.bt $child
  check_gsignal $child.bt $child
  check_err $child.err $child
  core="core.`ulimit -c unlimited; set +e; ./$child --gencore --run; true`"
  tempfiles $core{,.bt,.err}
  ./backtrace ./$child ./$core 1>$core.bt 2>$core.err \
    || true
  cat $core.{bt,err}
  check_main $core.bt $child-$core
  check_gsignal $core.bt $child-$core
  check_err $core.err $child-$core
done

for arch in ppc ppc64 s390 s390x; do
  testfiles backtrace.$arch.{exec,core}
  tempfiles backtrace.$arch.{bt,err}
  echo ./backtrace ./backtrace.$arch.{exec,core}
  ./backtrace ./backtrace.$arch.{exec,core} 1>backtrace.$arch.bt  \
					    2>backtrace.$arch.err \
    || true
  cat backtrace.$arch.{bt,err}
  check_main backtrace.$arch.bt $arch
  check_gsignal backtrace.$arch.bt $arch
  check_err backtrace.$arch.err $arch
done

exit 0
