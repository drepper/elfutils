/* Get previous frame state for an existing frame state.
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

#include <stdlib.h>
#include "../libdwfl/libdwfl.h"
#include <assert.h>
#include "../libdw/cfi.h"

#define BACKEND s390_
#include "libebl_CPU.h"

bool
s390_frame_unwind (Ebl *ebl, Dwarf_Frame_State **statep, Dwarf_Addr pc,
		   bool (*memory_read) (Dwarf_Frame_State_Process *process,
					Dwarf_Addr addr, Dwarf_Addr *result))
{
  Dwarf_Frame_State *state = *statep;
  Dwarf_Frame_State_Process *process = state->thread->process;
  assert (state->unwound == NULL);
  /* Caller already assumed caller adjustment but S390 instructions are 4 bytes
     long.  Undo it.  */
  if ((pc & 0x3) != 0x3)
    return false;
  pc++;
  /* We can assume big-endian read here.  */
  Dwarf_Addr instr;
  if (! memory_read (process, pc, &instr))
    return false;
  /* Fetch only the very first two bytes.  */
  instr = (instr >> (ebl->class == ELFCLASS64 ? 48 : 16)) & 0xffff;
  /* See GDB s390_sigtramp_frame_sniffer.  */
  /* Check for 'svc'.  */
  if (((instr >> 8) & 0xff) != 0x0a)
    return false;
  /* Check for 'sigreturn' or 'rt_sigreturn'.  */
  if ((instr & 0xff) != 119 && (instr & 0xff) != 173)
    return false;
  /* See GDB s390_sigtramp_frame_unwind_cache.  */
# define S390_SP_REGNUM (0 + 15) /* S390_R15_REGNUM */
  Dwarf_Addr this_sp;
  if (! dwarf_frame_state_reg_get (state, S390_SP_REGNUM, &this_sp))
    return false;
  unsigned word_size = ebl->class == ELFCLASS64 ? 8 : 4;
  Dwarf_Addr next_cfa = this_sp + 16 * word_size + 32;
  /* "New-style RT frame" is not supported,
     assuming "Old-style RT frame and all non-RT frames".  */
  Dwarf_Addr sigreg_ptr;
  if (! memory_read (process, next_cfa + 8, &sigreg_ptr))
    return false;
  /* Skip PSW mask.  */
  sigreg_ptr += word_size;
  /* Read PSW address.  */
  Dwarf_Addr val;
  if (! memory_read (process, sigreg_ptr, &val))
    return false;
  sigreg_ptr += word_size;
  size_t nregs = ebl->frame_state_nregs;
  Dwarf_Frame_State *unwound;
  unwound = malloc (sizeof (*unwound) + sizeof (*unwound->regs) * nregs);
  state->unwound = unwound;
  unwound->thread = state->thread;
  unwound->unwound = NULL;
  unwound->pc = val;
  unwound->pc_state = DWARF_FRAME_STATE_ERROR;
  memset (unwound->regs_set, 0, sizeof (unwound->regs_set));
  unwound->signal_frame = true;
  /* Then the GPRs.  */
  for (int i = 0; i < 16; i++)
    {
      if (! memory_read (process, sigreg_ptr, &val))
	return false;
      if (! dwarf_frame_state_reg_set (unwound, 0 + i, val))
	return false;
      sigreg_ptr += word_size;
    }
  /* Then the ACRs.  Skip them, they are not used in CFI.  */
  for (int i = 0; i < 16; i++)
    sigreg_ptr += 4;
  /* The floating-point control word.  */
  sigreg_ptr += 8;
  /* And finally the FPRs.  */
  for (int i = 0; i < 16; i++)
    {
      if (! memory_read (process, sigreg_ptr, &val))
	return false;
      if (ebl->class == ELFCLASS32)
	{
	  Dwarf_Addr val_low;
	  if (! memory_read (process, sigreg_ptr + 4, &val_low))
	    return false;
	  val = (val << 32) | val_low;
	}
      if (! dwarf_frame_state_reg_set (unwound, 16 + i, val))
	return false;
      sigreg_ptr += 8;
    }
  /* If we have them, the GPR upper halves are appended at the end.  */
  if (ebl->class == ELFCLASS32)
    {
      unsigned sigreg_high_off = 4;
      sigreg_ptr += sigreg_high_off;
      for (int i = 0; i < 16; i++)
	{
	  if (! memory_read (process, sigreg_ptr, &val))
	    return false;
	  Dwarf_Addr val_low;
	  if (! dwarf_frame_state_reg_get (unwound, 0 + i, &val_low))
	    return false;
	  val = (val << 32) | val_low;
	  if (! dwarf_frame_state_reg_set (unwound, 0 + i, val))
	    return false;
	  sigreg_ptr += 4;
	}
    }
  unwound->pc_state = DWARF_FRAME_STATE_PC_SET;
  *statep = unwound;
  return true;
}
