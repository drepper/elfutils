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

testrun_compare ./dwarf_edit <<\EOF
consed:
 <compile_unit name="source-file.c">
  <base_type ref="ref1" name="int"/>
  <subprogram name="foo" external=1 type="#ref1" description="foo"/>
 </compile_unit>
EOF

exit 0
