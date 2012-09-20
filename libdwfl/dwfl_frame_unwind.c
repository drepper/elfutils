/* Get return address register for frame.
   Copyright (C) 2009 Red Hat, Inc.
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

#include "cfi.h"
#include <stdlib.h>
#include "libdwflP.h"

bool
state_get_reg (Dwarf_Frame_State *state, unsigned regno, uint64_t *val)
{
  if (regno >= state->base->nregs)
    return false;
  if (((1U << regno) & state->regs_set) == 0)
    return false;
  *val = state->regs[regno];
  return true;
}

bool
get_cfa (Dwarf_Frame_State *state, Dwarf_Frame *frame, Dwarf_Addr *cfa)
{
  /* The CFA is unknown, is R+N, or is computed by a DWARF expression.
     A bogon in the CFI can indicate an invalid/incalculable rule.
     We store that as cfa_invalid rather than barfing when processing it,
     so callers can ignore the bogon unless they really need that CFA.  */
  switch (frame->cfa_rule)
  {
    case cfa_undefined:
      return false;
    case cfa_offset:
      {
	uint64_t val;
	if (state_get_reg (state, frame->cfa_val_reg, &val))
	  {
	    *cfa = val + frame->cfa_val_offset;
	    return true;
	  }
	return false;
      }
    case cfa_expr:
      /* FIXME - UNIMPLEMENTED - frame->cfa_data.expr */
      return false;
    case cfa_invalid:
      return false;
    default:
      abort ();
  }
}

/* Exact copy from libdwfl/segment.c.  */

static GElf_Addr
segment_start (Dwfl *dwfl, GElf_Addr start)
{
  if (dwfl->segment_align > 1)
    start &= -dwfl->segment_align;
  return start;
}

/* Exact copy from libdwfl/segment.c.  */

static GElf_Addr
segment_end (Dwfl *dwfl, GElf_Addr end)
{
  if (dwfl->segment_align > 1)
    end = (end + dwfl->segment_align - 1) & -dwfl->segment_align;
  return end;
}

bool
memory_read (Dwarf_Frame_State *state, Dwarf_Addr addr, unsigned long *ul)
{
  if (state->base->pid)
    return ebl_memory_read (state->base->ebl, state->base->pid, addr, ul);

  if (state->base->core)
    {
      Elf *core = state->base->core;
      Dwfl *dwfl = state->base->dwfl;
      static size_t phnum;
      if (elf_getphdrnum (core, &phnum) < 0)
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return false;
	}
      for (size_t cnt = 0; cnt < phnum; ++cnt)
	{
	  GElf_Phdr phdr_mem, *phdr = gelf_getphdr (core, cnt, &phdr_mem);
	  if (phdr == NULL || phdr->p_type != PT_LOAD)
	    continue;
	  /* FIXME */
	  Dwarf_Addr bias = 0;
	  GElf_Addr start = segment_start (dwfl, bias + phdr->p_vaddr);
	  GElf_Addr end = segment_end (dwfl, bias + phdr->p_vaddr + phdr->p_memsz);
	  if (addr < start || addr + sizeof (*ul) > end)
	    continue;
	  Elf_Data *data = elf_getdata_rawchunk (core, phdr->p_offset + addr - start, sizeof (*ul), ELF_T_ADDR);
	  if (data == NULL)
	    {
	      __libdwfl_seterrno (DWFL_E_LIBELF);
	      return false;
	    }
	  assert (data->d_size == sizeof (*ul));
	  *ul = *(const unsigned long *) data->d_buf;
	  return true;
	}
      return false;
    }

  abort ();
}

static Dwarf_Frame_State *
handle_cfi (Dwarf_Frame_State *state, Dwarf_Addr pc, Dwfl_Module *mod __attribute__ ((unused)), Dwarf_CFI *cfi, Dwarf_Addr bias __attribute__ ((unused)))
{
  Dwarf_Frame *frame;
  if (dwarf_cfi_addrframe (cfi, pc, &frame) != 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return false;
    }
  Dwarf_Frame_State *unwound = malloc (sizeof (*unwound) + sizeof (*unwound->regs) * state->base->nregs);
  state->unwound = unwound;
  unwound->base = state->base;
  unwound->unwound = NULL;
  unwound->regs_set = 0;
  bool fail = false;
  for (unsigned regno = 0; regno < unwound->base->nregs; regno++)
    {
      const struct dwarf_frame_register *reg = frame->regs + regno;
      switch (reg->rule)
      {
	case reg_undefined:		/* DW_CFA_undefined */
	  break;
	case reg_unspecified:		/* Uninitialized state.  */
	  /* PASSTHRU - this is the common GCC violation of DWARF.  */
	case reg_same_value:		/* DW_CFA_same_value */
	  {
	    uint64_t val;
	    if (state_get_reg (state, regno, &val))
	      {
		unwound->regs[regno] = val;
		unwound->regs_set |= 1U << regno;
		break;
	      }
	  }
	  fail = true;
	  break;
	case reg_offset:		/* DW_CFA_offset_extended et al */
	  {
	    Dwarf_Addr cfa;
	    if (get_cfa (state, frame, &cfa))
	      {
		Dwarf_Addr addr = cfa + reg->value;
		unsigned long ul;
		if (memory_read (state, addr, &ul))
		  {
		    unwound->regs[regno] = ul;
		    unwound->regs_set |= 1U << regno;
		    break;
		  }
	      }
	  }
	  fail = true;
	  break;
	case reg_val_offset:		/* DW_CFA_val_offset et al */
	  {
	    Dwarf_Addr cfa;
	    if (get_cfa (state, frame, &cfa))
	      {
		unwound->regs[regno] = cfa + reg->value;
		unwound->regs_set |= 1U << regno;
		break;
	      }
	  }
	  fail = true;
	  break;
	case reg_register:		/* DW_CFA_register */
	  {
	    uint64_t val;
	    if (state_get_reg (state, reg->value, &val))
	      {
		unwound->regs[regno] = val;
		unwound->regs_set |= 1U << regno;
		break;
	      }
	  }
	  fail = true;
	  break;
	case reg_expression:		/* DW_CFA_expression */
	/*
	expression(E)		section offset of DW_FORM_block containing E
					(register saved at address E computes)
	*/
	  /* FIXME - UNIMPLEMENTED */
	  fail = true;
	  break;
	case reg_val_expression:	/* DW_CFA_val_expression */
	/*
	val_expression(E)	section offset of DW_FORM_block containing E
	*/
	  /* FIXME - UNIMPLEMENTED */
	  fail = true;
	  break;
	default:
	  abort ();
      }
    }
  if (fail)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return NULL;
    }
  return unwound;
}

Dwarf_Frame_State *
dwfl_frame_unwind (Dwarf_Frame_State *state)
{
  assert (state->unwound == NULL);
  Dwarf_Addr pc;
  pc = dwarf_frame_state_pc (state);
  int dw_errno = dwarf_errno ();
  if (dw_errno != DWARF_E_NOERROR)
    {
      __libdw_seterrno (dw_errno);
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return NULL;
    }
  Dwfl_Module *mod = dwfl_addrmodule (state->base->dwfl, pc);
  if (mod == NULL)
    return NULL;
  Dwarf_Addr bias;
  Dwarf_CFI *cfi = dwfl_module_eh_cfi (mod, &bias);
  if (cfi)
    {
      Dwarf_Frame_State *unwound = handle_cfi (state, pc, mod, cfi, bias);
      if (unwound)
	return unwound;
    }
  cfi = dwfl_module_dwarf_cfi (mod, &bias);
  if (cfi)
    return handle_cfi (state, pc, mod, cfi, bias);
  return NULL;
}
INTDEF(dwfl_frame_unwind)
