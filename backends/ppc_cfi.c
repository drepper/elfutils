/* ppc ABI-specified defaults for DWARF CFI.
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

#define BACKEND ppc_
#include "libebl_CPU.h"

int
ppc_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* This only instruction is provided in every PPC32 CIE.  */
      DW_CFA_def_cfa, ULEB128_7 (1), ULEB128_7 (0),
      /* These rules are assumed by PPC32 FDEs, without specifying these.  */
      /* r1 is restored from cfa adress, r1 acts as a stack frame pointer.  */
      DW_CFA_val_expression, ULEB128_7 (1), ULEB128_7 (1), DW_OP_nop,
      /* Some FDEs do not specify %lr but inherit it.  */
      DW_CFA_same_value, ULEB128_7 (65), /* %lr */

      /* At least %r1 is inherited on PPC64, inherit all %gprs.  */
#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      SV (0), SV (1), SV (2), SV (3), SV (4), SV (5), SV (6), SV (7), SV (8),
      SV (9), SV (10), SV (11), SV (12), SV (13), SV (14), SV (15), SV (16),
      SV (17), SV (18), SV (19), SV (20), SV (21), SV (22), SV (23), SV (24),
      SV (25), SV (26), SV (27), SV (28), SV (29), SV (30), SV (31)
#undef SV
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = 4;

  abi_info->return_address_register = 65;

  return 0;
}

int
ppc64_abi_cfi (Ebl *ebl, Dwarf_CIE *abi_info)
{
  ppc_abi_cfi (ebl, abi_info);

  abi_info->data_alignment_factor = 8;

  return 0;
}
