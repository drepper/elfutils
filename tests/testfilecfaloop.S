/* Test program for run-cfaloop.sh.
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

	.file	"testfilecfaloop.c"
	.text
	.p2align 4,,15
	.globl	_start
	.type	_start, @function
_start:
.LFB0:
# BLOCK 2 freq:10000 seq:0
# PRED: ENTRY [100.0%]  (FALLTHRU)
	movq	$0, -8(%rsp)
	movq	-8(%rsp), %rax
	movl	$0, (%rax)
# SUCC: EXIT [100.0%] 
	ret
.LFE0:
	.size	_start, .-_start
#APP
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1	# Length of Common Information Entry
.LSCIE1:
	.long	0	# CIE Identifier Tag
	.byte	0x3	# CIE Version
	.ascii "zR\0"	# CIE Augmentation
	.uleb128 0x1	# CIE Code Alignment Factor
	.sleb128 -8	# CIE Data Alignment Factor
	.uleb128 0x10	# CIE RA Column
	.uleb128 0x1	# Augmentation size
	.byte	0x3	# FDE Encoding (udata4)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90	# DW_CFA_offset, column 0x10
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1	# FDE Length
.LASFDE1:
	.long	.LASFDE1-.Lframe1	# FDE CIE offset
	.long	.LFB0	# FDE initial location
	.long	.LFE0-.LFB0	# FDE address range
	.uleb128 0	# Augmentation size
// BEGIN inserted data { DW_CFA_same_value + ULEB128 register }
#define REG(n)		\
	.byte	0x8;	\
	.uleb128 n;
REG(0)
REG(1)
REG(2)
REG(3)
REG(4)
REG(5)
REG(6)
REG(7)
REG(8)
REG(9)
REG(10)
REG(11)
REG(12)
REG(13)
REG(14)
REG(15)
REG(16)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0x7
	.uleb128 0x0
// END inserted data
	.align 8
.LEFDE1:
#NO_APP
	.ident	"GCC: (GNU) 4.8.2 20131212 (Red Hat 4.8.2-7)"
	.section	.note.GNU-stack,"",@progbits
