#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# = funcs.s =
# .globl testfunc
# testfunc:
# 	nop
# 	ret
# .type testfunc, @function
# .size testfunc, .-testfunc
#
# .globl testfunc2
# testfunc2:
# 	call testfunc
# 	nop
# 	nop
# 	ret
# .type testfunc2, @function
# .size testfunc2, .-testfunc2
#
# .globl functest3
# functest3:
# 	jmp local
# 	nop
# 	nop
# local:
# 	call testfunc2
# 	ret
# .type functest3, @function
# .size functest3, .-functest3

# = start.s =
# .global _start
# _start:
# 	call functest3
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	nop
# 	ret
# .type _start, @function
# .size _start, .-_start

# gas --compress-debug-sections=zlib-gnu -32 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gnu -32 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gnu -melf_i386 -g -o zgnu32 funcs.o start.o

# gas --compress-debug-sections=zlib-gnu -64 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gnu -64 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gnu -g -o zgnu32 funcs.o start.o

# gas --compress-debug-sections=zlib-gabi -32 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gabi -32 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gabi -melf_i386 -g -o zgabi32 funcs.o start.o

# gas --compress-debug-sections=zlib-gabi -64 -g -o start.o start.s
# gas --compress-debug-sections=zlib-gabi -64 -g -o funcs.o funcs.s
# ld --compress-debug-sections=zlib-gabi -g -o zgabi64 funcs.o start.o

testfiles testfile-zgnu64
testrun_compare ${abs_top_builddir}/tests/elfgetchdr testfile-zgnu64 <<\EOF
section 1: NOT Compressed section does not contain compressed data
section 2: GNU Compressed ch_type: 1, ch_size: 60, ch_addralign: 10
section 3: GNU Compressed ch_type: 1, ch_size: aa, ch_addralign: 1
section 4: NOT Compressed section does not contain compressed data
section 5: GNU Compressed ch_type: 1, ch_size: 8d, ch_addralign: 1
section 6: NOT Compressed section does not contain compressed data
section 7: NOT Compressed section does not contain compressed data
section 8: NOT Compressed section does not contain compressed data
EOF

testfiles testfile-zgabi64
testrun_compare ${abs_top_builddir}/tests/elfgetchdr testfile-zgabi64 <<\EOF
section 1: NOT Compressed section does not contain compressed data
section 2: ELF Compressed ch_type: 1, ch_size: 60, ch_addralign: 10
section 3: ELF Compressed ch_type: 1, ch_size: aa, ch_addralign: 1
section 4: NOT Compressed section does not contain compressed data
section 5: ELF Compressed ch_type: 1, ch_size: 8d, ch_addralign: 1
section 6: NOT Compressed section does not contain compressed data
section 7: NOT Compressed section does not contain compressed data
section 8: NOT Compressed section does not contain compressed data
EOF

testfiles testfile-zgnu32
testrun_compare ${abs_top_builddir}/tests/elfgetchdr testfile-zgnu32 <<\EOF
section 1: NOT Compressed section does not contain compressed data
section 2: GNU Compressed ch_type: 1, ch_size: 40, ch_addralign: 8
section 3: GNU Compressed ch_type: 1, ch_size: 9a, ch_addralign: 1
section 4: NOT Compressed section does not contain compressed data
section 5: GNU Compressed ch_type: 1, ch_size: 85, ch_addralign: 1
section 6: NOT Compressed section does not contain compressed data
section 7: NOT Compressed section does not contain compressed data
section 8: NOT Compressed section does not contain compressed data
EOF

testfiles testfile-zgabi32
testrun_compare ${abs_top_builddir}/tests/elfgetchdr testfile-zgabi32 <<\EOF
section 1: NOT Compressed section does not contain compressed data
section 2: ELF Compressed ch_type: 1, ch_size: 40, ch_addralign: 8
section 3: ELF Compressed ch_type: 1, ch_size: 9a, ch_addralign: 1
section 4: NOT Compressed section does not contain compressed data
section 5: ELF Compressed ch_type: 1, ch_size: 85, ch_addralign: 1
section 6: NOT Compressed section does not contain compressed data
section 7: NOT Compressed section does not contain compressed data
section 8: NOT Compressed section does not contain compressed data
EOF

exit 0
