/* dwarf-opcodes
   Copyright (C) 2009-2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#define DW_OP_OPERANDS						\
  DW_OP_1 (DW_OP_addr, DW_FORM_addr)				\
  DW_OP_0 (DW_OP_deref)						\
  DW_OP_0 (DW_OP_xderef)					\
  DW_OP_1 (DW_OP_deref_size, DW_FORM_data1)			\
  DW_OP_1 (DW_OP_xderef_size, DW_FORM_data1)			\
  DW_OP_1 (DW_OP_const1u, DW_FORM_data1)			\
  DW_OP_1 (DW_OP_const1s, DW_FORM_data1)			\
  DW_OP_1 (DW_OP_const2u, DW_FORM_data2)			\
  DW_OP_1 (DW_OP_const2s, DW_FORM_data2)			\
  DW_OP_1 (DW_OP_const4u, DW_FORM_data4)			\
  DW_OP_1 (DW_OP_const4s, DW_FORM_data4)			\
  DW_OP_1 (DW_OP_const8u, DW_FORM_data8)			\
  DW_OP_1 (DW_OP_const8s, DW_FORM_data8)			\
  DW_OP_1 (DW_OP_constu, DW_FORM_udata)				\
  DW_OP_1 (DW_OP_consts, DW_FORM_sdata)				\
  DW_OP_0 (DW_OP_dup)						\
  DW_OP_0 (DW_OP_drop)						\
  DW_OP_0 (DW_OP_over)						\
  DW_OP_1 (DW_OP_pick, DW_FORM_data1)				\
  DW_OP_0 (DW_OP_swap)						\
  DW_OP_0 (DW_OP_rot)						\
  DW_OP_0 (DW_OP_abs)						\
  DW_OP_0 (DW_OP_and)						\
  DW_OP_0 (DW_OP_div)						\
  DW_OP_0 (DW_OP_minus)						\
  DW_OP_0 (DW_OP_mod)						\
  DW_OP_0 (DW_OP_mul)						\
  DW_OP_0 (DW_OP_neg)						\
  DW_OP_0 (DW_OP_not)						\
  DW_OP_0 (DW_OP_or)						\
  DW_OP_0 (DW_OP_plus)						\
  DW_OP_1 (DW_OP_plus_uconst, DW_FORM_udata)			\
  DW_OP_0 (DW_OP_shl)						\
  DW_OP_0 (DW_OP_shr)						\
  DW_OP_0 (DW_OP_shra)						\
  DW_OP_0 (DW_OP_xor)						\
  DW_OP_1 (DW_OP_bra, DW_FORM_data2)				\
  DW_OP_0 (DW_OP_eq)						\
  DW_OP_0 (DW_OP_ge)						\
  DW_OP_0 (DW_OP_gt)						\
  DW_OP_0 (DW_OP_le)						\
  DW_OP_0 (DW_OP_lt)						\
  DW_OP_0 (DW_OP_ne)						\
  DW_OP_1 (DW_OP_skip, DW_FORM_data2)				\
  DW_OP_0 (DW_OP_lit0)						\
  DW_OP_0 (DW_OP_lit1)						\
  DW_OP_0 (DW_OP_lit2)						\
  DW_OP_0 (DW_OP_lit3)						\
  DW_OP_0 (DW_OP_lit4)						\
  DW_OP_0 (DW_OP_lit5)						\
  DW_OP_0 (DW_OP_lit6)						\
  DW_OP_0 (DW_OP_lit7)						\
  DW_OP_0 (DW_OP_lit8)						\
  DW_OP_0 (DW_OP_lit9)						\
  DW_OP_0 (DW_OP_lit10)						\
  DW_OP_0 (DW_OP_lit11)						\
  DW_OP_0 (DW_OP_lit12)						\
  DW_OP_0 (DW_OP_lit13)						\
  DW_OP_0 (DW_OP_lit14)						\
  DW_OP_0 (DW_OP_lit15)						\
  DW_OP_0 (DW_OP_lit16)						\
  DW_OP_0 (DW_OP_lit17)						\
  DW_OP_0 (DW_OP_lit18)						\
  DW_OP_0 (DW_OP_lit19)						\
  DW_OP_0 (DW_OP_lit20)						\
  DW_OP_0 (DW_OP_lit21)						\
  DW_OP_0 (DW_OP_lit22)						\
  DW_OP_0 (DW_OP_lit23)						\
  DW_OP_0 (DW_OP_lit24)						\
  DW_OP_0 (DW_OP_lit25)						\
  DW_OP_0 (DW_OP_lit26)						\
  DW_OP_0 (DW_OP_lit27)						\
  DW_OP_0 (DW_OP_lit28)						\
  DW_OP_0 (DW_OP_lit29)						\
  DW_OP_0 (DW_OP_lit30)						\
  DW_OP_0 (DW_OP_lit31)						\
  DW_OP_0 (DW_OP_reg0)						\
  DW_OP_0 (DW_OP_reg1)						\
  DW_OP_0 (DW_OP_reg2)						\
  DW_OP_0 (DW_OP_reg3)						\
  DW_OP_0 (DW_OP_reg4)						\
  DW_OP_0 (DW_OP_reg5)						\
  DW_OP_0 (DW_OP_reg6)						\
  DW_OP_0 (DW_OP_reg7)						\
  DW_OP_0 (DW_OP_reg8)						\
  DW_OP_0 (DW_OP_reg9)						\
  DW_OP_0 (DW_OP_reg10)						\
  DW_OP_0 (DW_OP_reg11)						\
  DW_OP_0 (DW_OP_reg12)						\
  DW_OP_0 (DW_OP_reg13)						\
  DW_OP_0 (DW_OP_reg14)						\
  DW_OP_0 (DW_OP_reg15)						\
  DW_OP_0 (DW_OP_reg16)						\
  DW_OP_0 (DW_OP_reg17)						\
  DW_OP_0 (DW_OP_reg18)						\
  DW_OP_0 (DW_OP_reg19)						\
  DW_OP_0 (DW_OP_reg20)						\
  DW_OP_0 (DW_OP_reg21)						\
  DW_OP_0 (DW_OP_reg22)						\
  DW_OP_0 (DW_OP_reg23)						\
  DW_OP_0 (DW_OP_reg24)						\
  DW_OP_0 (DW_OP_reg25)						\
  DW_OP_0 (DW_OP_reg26)						\
  DW_OP_0 (DW_OP_reg27)						\
  DW_OP_0 (DW_OP_reg28)						\
  DW_OP_0 (DW_OP_reg29)						\
  DW_OP_0 (DW_OP_reg30)						\
  DW_OP_0 (DW_OP_reg31)						\
  DW_OP_1 (DW_OP_breg0, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg1, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg2, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg3, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg4, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg5, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg6, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg7, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg8, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg9, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg10, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg11, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg12, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg13, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg14, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg15, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg16, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg17, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg18, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg19, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg20, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg21, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg22, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg23, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg24, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg25, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg26, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg27, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg28, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg29, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg30, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_breg31, DW_FORM_sdata)				\
  DW_OP_1 (DW_OP_regx, DW_FORM_udata)				\
  DW_OP_1 (DW_OP_fbreg, DW_FORM_sdata)				\
  DW_OP_2 (DW_OP_bregx, DW_FORM_udata, DW_FORM_sdata)		\
  DW_OP_1 (DW_OP_piece, DW_FORM_udata)				\
  DW_OP_0 (DW_OP_nop)						\
  DW_OP_0 (DW_OP_push_object_address)				\
  DW_OP_1 (DW_OP_call2, DW_FORM_data2)				\
  DW_OP_1 (DW_OP_call4, DW_FORM_data4)				\
  DW_OP_1 (DW_OP_call_ref, DW_FORM_ref_addr)			\
  DW_OP_0 (DW_OP_form_tls_address)				\
  DW_OP_0 (DW_OP_GNU_push_tls_address)				\
  DW_OP_0 (DW_OP_call_frame_cfa)				\
  DW_OP_2 (DW_OP_bit_piece, DW_FORM_udata, DW_FORM_udata)	\
  DW_OP_0 (DW_OP_GNU_uninit)					\
  /* DWARF 4 */							\
  DW_OP_0 (DW_OP_stack_value)					\
  DW_OP_1 (DW_OP_implicit_value, DW_FORM_block)			\
  /* GNU extensions */						\
  DW_OP_2 (DW_OP_GNU_implicit_pointer, DW_FORM_ref_addr, DW_FORM_sdata) \
  /* GNU variant for tracking of values passed as arguments to functions.  */ \
  /* http://www.dwarfstd.org/ShowIssue.php?issue=100909.1 */	\
  DW_OP_1 (DW_OP_GNU_entry_value, DW_FORM_block)		\
  /* The GNU typed stack extension.  */				\
  /* See http://www.dwarfstd.org/doc/040408.1.html */		\
  DW_OP_2 (DW_OP_GNU_const_type, DW_FORM_udata, DW_FORM_block1)	\
  DW_OP_2 (DW_OP_GNU_regval_type, DW_FORM_udata, DW_FORM_udata)	\
  DW_OP_2 (DW_OP_GNU_deref_type, DW_FORM_data1, DW_FORM_udata)	\
  DW_OP_1 (DW_OP_GNU_convert, DW_FORM_udata)			\
  DW_OP_1 (DW_OP_GNU_parameter_ref, DW_FORM_ref4) \
  DW_OP_1 (DW_OP_GNU_reinterpret, DW_FORM_udata)
