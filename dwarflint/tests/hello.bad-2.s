	.file	"hello.c"
# GNU C (GCC) version 4.5.1 20100924 (Red Hat 4.5.1-4) (x86_64-redhat-linux)
#	compiled by GNU C version 4.5.1 20100924 (Red Hat 4.5.1-4), GMP version 4.3.1, MPFR version 2.4.2, MPC version 0.8.1
# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed:  hello.c -mtune=generic -march=x86-64 -g -fverbose-asm
# options enabled:  -falign-loops -fargument-alias
# -fasynchronous-unwind-tables -fauto-inc-dec -fbranch-count-reg -fcommon
# -fdelete-null-pointer-checks -fdwarf2-cfi-asm -fearly-inlining
# -feliminate-unused-debug-types -ffunction-cse -fgcse-lm -fident
# -finline-functions-called-once -fira-share-save-slots
# -fira-share-spill-slots -fivopts -fkeep-static-consts
# -fleading-underscore -fmath-errno -fmerge-debug-strings
# -fmove-loop-invariants -fpeephole -freg-struct-return
# -fsched-critical-path-heuristic -fsched-dep-count-heuristic
# -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
# -fsched-rank-heuristic -fsched-spec -fsched-spec-insn-heuristic
# -fsched-stalled-insns-dep -fshow-column -fsigned-zeros
# -fsplit-ivs-in-unroller -ftrapping-math -ftree-cselim -ftree-forwprop
# -ftree-loop-im -ftree-loop-ivcanon -ftree-loop-optimize
# -ftree-parallelize-loops= -ftree-phiprop -ftree-pta -ftree-reassoc
# -ftree-scev-cprop -ftree-slp-vectorize -ftree-vect-loop-version
# -funit-at-a-time -funwind-tables -fvect-cost-model -fverbose-asm
# -fzero-initialized-in-bss -m128bit-long-double -m64 -m80387
# -maccumulate-outgoing-args -malign-stringops -mfancy-math-387
# -mfp-ret-in-387 -mfused-madd -mglibc -mieee-fp -mmmx -mno-sse4
# -mpush-args -mred-zone -msse -msse2 -mtls-direct-seg-refs

	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.text
.Ltext0:
# Compiler executable checksum: ea394b69293dd698607206e8e43d607e

.globl main
	.type	main, @function
main:
.LFB0:
	.file 1 "hello.c"
	# hello.c:3
	.loc 1 3 0
	.cfi_startproc
	# basic block 2
	pushq	%rbp	#	# 15	*pushdi2_rex64/1	[length = 1]
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,	# 16	*movdi_1_rex64/2	[length = 3]
	.cfi_def_cfa_register 6
	movl	%edi, -4(%rbp)	# argc, argc	# 2	*movsi_1/2	[length = 3]
	movq	%rsi, -16(%rbp)	# argv, argv	# 3	*movdi_1_rex64/4	[length = 4]
	# hello.c:4
	.loc 1 4 0
	leave	# 21	leave_rex64	[length = 1]
	.cfi_def_cfa 7, 8
	ret	# 22	return_internal	[length = 1]
	.cfi_endproc
.LFE0:
	.size	main, .-main
.Letext0:
	.section	.debug_info
	.long	0x84	# Length of Compilation Unit Info
	.value	0x3	# DWARF version number
	.long	.Ldebug_abbrev0	# Offset Into Abbrev. Section
	.byte	0x8	# Pointer Size (in bytes)
	.uleb128 0x1	# (DIE (0xb) DW_TAG_compile_unit)
	.long	.LASF3	# DW_AT_producer: "GNU C 4.5.1 20100924 (Red Hat 4.5.1-4)"
	.byte	0x1	# DW_AT_language
	.long	.LASF4	# DW_AT_name: "hello.c"
	.long	.LASF5	# DW_AT_comp_dir: "/home/mark/src/tests"
	.quad	.Ltext0	# DW_AT_low_pc
	.quad	.Letext0	# DW_AT_high_pc
	.long	.Ldebug_line0	# DW_AT_stmt_list
	.uleb128 0x2	# (DIE (0x2d) DW_TAG_subprogram)
	.byte	0x1	# DW_AT_external
	.long	.LASF6	# DW_AT_name: "main"
	.byte	0x1	# DW_AT_decl_file (hello.c)
	.byte	0x2	# DW_AT_decl_line
	.byte	0x1	# DW_AT_prototyped
	.long	0x6d	# DW_AT_type
	.quad	.LFB0	# DW_AT_low_pc
	.quad	.LFE0	# DW_AT_high_pc
	.byte	0x1	# DW_AT_frame_base
	.byte	0x9c	# DW_OP_call_frame_cfa
	.long	0x6d	# DW_AT_sibling
	.uleb128 0x3	# (DIE (0x50) DW_TAG_formal_parameter)
	.long	.LASF0	# DW_AT_name: "argc"
	.byte	0x1	# DW_AT_decl_file (hello.c)
	.byte	0x2	# DW_AT_decl_line
	.long	0x6d	# DW_AT_type
	.byte	0x2	# DW_AT_location
	.byte	0x91	# DW_OP_fbreg
	.sleb128 -20
	.uleb128 0x3	# (DIE (0x5e) DW_TAG_formal_parameter)
	.long	.LASF1	# DW_AT_name: "argv"
	.byte	0x1	# DW_AT_decl_file (hello.c)
	.byte	0x2	# DW_AT_decl_line
	.long	0x74	# DW_AT_type
	.byte	0x2	# DW_AT_location
	.byte	0x91	# DW_OP_fbreg
	.sleb128 -32
	.byte	0x0	# end of children of DIE 0x2d
	.uleb128 0x4	# (DIE (0x6d) DW_TAG_base_type)
	.byte	0x4	# DW_AT_byte_size
	.byte	0x5	# DW_AT_encoding
	.ascii "int\0"	# DW_AT_name
	.uleb128 0x5	# (DIE (0x74) DW_TAG_pointer_type)
	.byte	0x8	# DW_AT_byte_size
	.long	0x7a	# DW_AT_type
	.uleb128 0x5	# (DIE (0x7a) DW_TAG_pointer_type)
	.byte	0x8	# DW_AT_byte_size
	.long	0x80	# DW_AT_type
	.uleb128 0x6	# (DIE (0x80) DW_TAG_base_type)
	.byte	0x1	# DW_AT_byte_size
	.byte	0x6	# DW_AT_encoding
	.long	.LASF2	# DW_AT_name: "char"
	.byte	0x0	# end of children of DIE 0xb
	.section	.debug_abbrev
	.uleb128 0x1	# (abbrev code)
	.uleb128 0x11	# (TAG: DW_TAG_compile_unit)
	.byte	0x1	# DW_children_yes
	.uleb128 0x25	# (DW_AT_producer)
	.uleb128 0xe	# (DW_FORM_strp)
	.uleb128 0x13	# (DW_AT_language)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3	# (DW_AT_name)
	.uleb128 0xe	# (DW_FORM_strp)
	.uleb128 0x1b	# (DW_AT_comp_dir)
	.uleb128 0xe	# (DW_FORM_strp)
	.uleb128 0x11	# (DW_AT_low_pc)
	.uleb128 0x1	# (DW_FORM_addr)
	.uleb128 0x12	# (DW_AT_high_pc)
	.uleb128 0x1	# (DW_FORM_addr)
	.byte	0x0
	.byte	0x0
	.uleb128 0x2	# (abbrev code)
	.uleb128 0x2e	# (TAG: DW_TAG_subprogram)
	.byte	0x1	# DW_children_yes
	.uleb128 0x3f	# (DW_AT_external)
	.uleb128 0xc	# (DW_FORM_flag)
	.uleb128 0x3	# (DW_AT_name)
	.uleb128 0xe	# (DW_FORM_strp)
	.uleb128 0x3a	# (DW_AT_decl_file)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3b	# (DW_AT_decl_line)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x27	# (DW_AT_prototyped)
	.uleb128 0xc	# (DW_FORM_flag)
	.uleb128 0x49	# (DW_AT_type)
	.uleb128 0x13	# (DW_FORM_ref4)
	.uleb128 0x11	# (DW_AT_low_pc)
	.uleb128 0x1	# (DW_FORM_addr)
	.uleb128 0x12	# (DW_AT_high_pc)
	.uleb128 0x1	# (DW_FORM_addr)
	.uleb128 0x40	# (DW_AT_frame_base)
	.uleb128 0xa	# (DW_FORM_block1)
	.uleb128 0x1	# (DW_AT_sibling)
	.uleb128 0x13	# (DW_FORM_ref4)
	.byte	0x0
	.byte	0x0
	.uleb128 0x3	# (abbrev code)
	.uleb128 0x5	# (TAG: DW_TAG_formal_parameter)
	.byte	0x0	# DW_children_no
	.uleb128 0x3	# (DW_AT_name)
	.uleb128 0xe	# (DW_FORM_strp)
	.uleb128 0x3a	# (DW_AT_decl_file)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3b	# (DW_AT_decl_line)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x49	# (DW_AT_type)
	.uleb128 0x13	# (DW_FORM_ref4)
	.uleb128 0x2	# (DW_AT_location)
	.uleb128 0xa	# (DW_FORM_block1)
	.byte	0x0
	.byte	0x0
	.uleb128 0x4	# (abbrev code)
	.uleb128 0x24	# (TAG: DW_TAG_base_type)
	.byte	0x0	# DW_children_no
	.uleb128 0xb	# (DW_AT_byte_size)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3e	# (DW_AT_encoding)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3	# (DW_AT_name)
	.uleb128 0x8	# (DW_FORM_string)
	.byte	0x0
	.byte	0x0
	.uleb128 0x5	# (abbrev code)
	.uleb128 0xf	# (TAG: DW_TAG_pointer_type)
	.byte	0x0	# DW_children_no
	.uleb128 0xb	# (DW_AT_byte_size)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x49	# (DW_AT_type)
	.uleb128 0x13	# (DW_FORM_ref4)
	.byte	0x0
	.byte	0x0
	.uleb128 0x6	# (abbrev code)
	.uleb128 0x24	# (TAG: DW_TAG_base_type)
	.byte	0x0	# DW_children_no
	.uleb128 0xb	# (DW_AT_byte_size)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3e	# (DW_AT_encoding)
	.uleb128 0xb	# (DW_FORM_data1)
	.uleb128 0x3	# (DW_AT_name)
	.uleb128 0xe	# (DW_FORM_strp)
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.section	.debug_pubnames,"",@progbits
	.long	0x17	# Length of Public Names Info
	.value	0x2	# DWARF Version
	.long	.Ldebug_info0	# Offset of Compilation Unit Info
	.long	0x88	# Compilation Unit Length
	.long	0x2d	# DIE offset
	.ascii "main\0"	# external name
	.long	0x0
	.section	.debug_aranges,"",@progbits
	.long	0x2c	# Length of Address Ranges Info
	.value	0x2	# DWARF Version
	.long	.Ldebug_info0	# Offset of Compilation Unit Info
	.byte	0x8	# Size of Address
	.byte	0x0	# Size of Segment Descriptor
	.value	0x0	# Pad to 16 byte boundary
	.value	0x0
	.quad	.Ltext0	# Address
	.quad	.Letext0-.Ltext0	# Length
	.quad	0x0
	.quad	0x0
	.section	.debug_str,"MS",@progbits,1
.LASF1:
	.string	"argv"
.LASF4:
	.string	"hello.c"
.LASF5:
	.string	"/home/mark/src/tests"
.LASF6:
	.string	"main"
.LASF0:
	.string	"argc"
.LASF3:
	.string	"GNU C 4.5.1 20100924 (Red Hat 4.5.1-4)"
.LASF2:
	.string	"char"
	.ident	"GCC: (GNU) 4.5.1 20100924 (Red Hat 4.5.1-4)"
	.section	.note.GNU-stack,"",@progbits
