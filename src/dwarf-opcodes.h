/*
   Copyright (C) 2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

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
  DW_OP_2 (DW_OP_GNU_implicit_pointer, DW_FORM_ref_addr, DW_FORM_sdata)
