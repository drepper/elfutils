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

# See run-elfgetchdr.sh for testfiles.

testfiles testfile-zgnu64
testrun_compare ${abs_top_builddir}/src/readelf -z -S testfile-zgnu64 <<\EOF
There are 9 section headers, starting at offset 0x3e0:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
     [Compression  Size     Al]
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .text                PROGBITS     0000000000400078 00000078 0000002a  0 AX     0   0  1
[ 2] .zdebug_aranges      PROGBITS     0000000000000000 00000260 00000032  0        0   0 16
     [GNU ZLIB     00000060   ]
[ 3] .zdebug_info         PROGBITS     0000000000000000 00000292 0000006f  0        0   0  1
     [GNU ZLIB     000000aa   ]
[ 4] .debug_abbrev        PROGBITS     0000000000000000 00000301 00000028  0        0   0  1
[ 5] .zdebug_line         PROGBITS     0000000000000000 00000329 0000005b  0        0   0  1
     [GNU ZLIB     0000008d   ]
[ 6] .shstrtab            STRTAB       0000000000000000 00000384 00000059  0        0   0  1
[ 7] .symtab              SYMTAB       0000000000000000 000000a8 00000168 24        8   8  8
[ 8] .strtab              STRTAB       0000000000000000 00000210 0000004b  0        0   0  1

EOF

testfiles testfile-zgabi64
testrun_compare ${abs_top_builddir}/src/readelf -z -S testfile-zgabi64 <<\EOF
There are 9 section headers, starting at offset 0x400:

Section Headers:
[Nr] Name                 Type         Addr             Off      Size     ES Flags Lk Inf Al
     [Compression  Size     Al]
[ 0]                      NULL         0000000000000000 00000000 00000000  0        0   0  0
[ 1] .text                PROGBITS     0000000000400078 00000078 0000002a  0 AX     0   0  1
[ 2] .debug_aranges       PROGBITS     0000000000000000 00000260 0000003e  0 C      0   0 16
     [ELF ZLIB (1) 00000060 16]
[ 3] .debug_info          PROGBITS     0000000000000000 0000029e 0000007b  0 C      0   0  1
     [ELF ZLIB (1) 000000aa  1]
[ 4] .debug_abbrev        PROGBITS     0000000000000000 00000319 00000028  0        0   0  1
[ 5] .debug_line          PROGBITS     0000000000000000 00000341 00000067  0 C      0   0  1
     [ELF ZLIB (1) 0000008d  1]
[ 6] .shstrtab            STRTAB       0000000000000000 000003a8 00000056  0        0   0  1
[ 7] .symtab              SYMTAB       0000000000000000 000000a8 00000168 24        8   8  8
[ 8] .strtab              STRTAB       0000000000000000 00000210 0000004b  0        0   0  1

EOF

testfiles testfile-zgnu32
testrun_compare ${abs_top_builddir}/src/readelf -z -S testfile-zgnu32 <<\EOF
There are 9 section headers, starting at offset 0x33c:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
     [Compression  Size   Al]
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .text                PROGBITS     08048054 000054 00002a  0 AX     0   0  1
[ 2] .zdebug_aranges      PROGBITS     00000000 0001c0 000031  0        0   0  8
     [GNU ZLIB     000040   ]
[ 3] .zdebug_info         PROGBITS     00000000 0001f1 00006f  0        0   0  1
     [GNU ZLIB     00009a   ]
[ 4] .debug_abbrev        PROGBITS     00000000 000260 000028  0        0   0  1
[ 5] .zdebug_line         PROGBITS     00000000 000288 00005a  0        0   0  1
     [GNU ZLIB     000085   ]
[ 6] .shstrtab            STRTAB       00000000 0002e2 000059  0        0   0  1
[ 7] .symtab              SYMTAB       00000000 000080 0000f0 16        8   8  4
[ 8] .strtab              STRTAB       00000000 000170 00004b  0        0   0  1

EOF

testfiles testfile-zgabi32
testrun_compare ${abs_top_builddir}/src/readelf -z -S testfile-zgabi32 <<\EOF
There are 9 section headers, starting at offset 0x338:

Section Headers:
[Nr] Name                 Type         Addr     Off    Size   ES Flags Lk Inf Al
     [Compression  Size   Al]
[ 0]                      NULL         00000000 000000 000000  0        0   0  0
[ 1] .text                PROGBITS     08048054 000054 00002a  0 AX     0   0  1
[ 2] .debug_aranges       PROGBITS     00000000 0001c0 000031  0 C      0   0  8
     [ELF ZLIB (1) 000040  8]
[ 3] .debug_info          PROGBITS     00000000 0001f1 00006f  0 C      0   0  1
     [ELF ZLIB (1) 00009a  1]
[ 4] .debug_abbrev        PROGBITS     00000000 000260 000028  0        0   0  1
[ 5] .debug_line          PROGBITS     00000000 000288 00005a  0 C      0   0  1
     [ELF ZLIB (1) 000085  1]
[ 6] .shstrtab            STRTAB       00000000 0002e2 000056  0        0   0  1
[ 7] .symtab              SYMTAB       00000000 000080 0000f0 16        8   8  4
[ 8] .strtab              STRTAB       00000000 000170 00004b  0        0   0  1

EOF

exit 0
