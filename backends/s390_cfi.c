/* s390 ABI-specified defaults for DWARF CFI.
   Copyright (C) 2012 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>

#define BACKEND s390_
#include "libebl_CPU.h"

int
s390_abi_cfi (Ebl *ebl, Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* This only instruction is provided in every S390 CIE.  */
      DW_CFA_def_cfa, ULEB128_7 (15), ULEB128_7 (96),
      /* These rules are assumed by S390 FDEs, without specifying these.  */
//      /* r1 is restored from cfa adress, r1 acts as a stack frame pointer.  */
//      DW_CFA_val_expression, ULEB128_7 (1), ULEB128_7 (1), DW_OP_nop,
      /* Some FDEs do not specify r14 and r15 but inherit it.  */
      DW_CFA_same_value, ULEB128_7 (14), /* r14 */
      DW_CFA_same_value, ULEB128_7 (15), /* r15 */

      /* At least r11 is inherited on S390, inherit all gprs.  */
#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      SV (0), SV (1), SV (2), SV (3), SV (4), SV (5), SV (6), SV (7), SV (8),
      SV (9), SV (10), SV (11), SV (12), SV (13), SV (14), SV (15)
#undef SV
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = ebl->class == ELFCLASS64 ? 8 : 4;

  abi_info->return_address_register = 14;

  return 0;
}

__typeof (s390_abi_cfi)
     s390x_abi_cfi
     __attribute__ ((alias ("s390_abi_cfi")));
