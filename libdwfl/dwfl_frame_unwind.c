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
#include "../libdw/dwarf.h"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

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

static bool
state_get_reg (Dwarf_Frame_State *state, unsigned regno, Dwarf_Addr *val)
{
  if (regno >= state->base->nregs || ((1U << regno) & state->regs_set) == 0)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
  *val = state->regs[regno];
  return true;
}

static bool
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

static bool
expr_eval (Dwarf_Frame_State *state, Dwarf_Frame *frame, const Dwarf_Op *ops, size_t nops, Dwarf_Addr *result)
{
  if (nops == 0)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return false;
    }
  Dwarf_Addr *stack = NULL;
  size_t stack_used = 0, stack_allocated = 0;
  bool
  push (Dwarf_Addr val)
  {
    if (stack_used == stack_allocated)
      {
	stack_allocated = MAX (stack_allocated * 2, 32);
	Dwarf_Addr *stack_new = realloc (stack, stack_allocated * sizeof (*stack));
	if (stack_new == NULL)
	  {
	    __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	    return false;
	  }
	stack = stack_new;
      }
    stack[stack_used++] = val;
    return true;
  }
  bool
  pop (Dwarf_Addr *val)
  {
    if (stack_used == 0)
      {
	__libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	return false;
      }
    *val = stack[--stack_used];
    return true;
  }
  Dwarf_Addr val1, val2;
  bool is_location = false;
  for (; nops--; ops++)
    switch (ops->atom)
    {
      case DW_OP_breg0 ... DW_OP_breg31:
	if (! state_get_reg (state, ops->atom - DW_OP_breg0, &val1))
	  {
	    free (stack);
	    return false;
	  }
	val1 += ops->number;
	if (! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_bregx:
	if (! state_get_reg (state, ops->number, &val1))
	  {
	    free (stack);
	    return false;
	  }
	val1 += ops->number2;
	if (! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_lit0 ... DW_OP_lit31:
	if (! push (ops->atom - DW_OP_lit0))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_and:
	if (! pop (&val2) || ! pop (&val1) || ! push (val1 & val2))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_ge:
	if (! pop (&val2) || ! pop (&val1) || ! push (val1 >= val2))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_shl:
	if (! pop (&val2) || ! pop (&val1) || ! push (val1 << val2))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_plus:
	if (! pop (&val2) || ! pop (&val1) || ! push (val1 + val2))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_plus_uconst:
	if (! pop (&val1) || ! push (val1 + ops->number))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_call_frame_cfa:
	{
	  Dwarf_Op *cfa_ops;
	  size_t cfa_nops;
	  if (dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops) != 0)
	    {
	      __libdwfl_seterrno (DWFL_E_LIBDW);
	      free (stack);
	      return false;
	    }
	  Dwarf_Addr cfa;
	  if (! expr_eval (state, frame, cfa_ops, cfa_nops, &cfa) || ! push (cfa))
	    {
	      free (stack);
	      return false;
	    }
	  is_location = true;
	}
	break;
      case DW_OP_stack_value:
	  is_location = false;
	break;
      default:
	__libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	return false;
    }
  bool retval = pop (result);
  free (stack);
  if (is_location)
    {
      unsigned long ul;
      if (! memory_read (state, *result, &ul))
	return false;
      *result = ul;
    }
  return retval;
}

static Dwarf_Frame_State *
have_unwound (Dwarf_Frame_State *unwound)
{
  dwarf_frame_state_pc (unwound);
  int dw_errno = dwarf_errno ();
  if (dw_errno == DWARF_E_RA_UNDEFINED)
    {
      __libdwfl_seterrno (DWFL_E_RA_UNDEFINED);
      return NULL;
    }
  if (dw_errno != DWARF_E_NOERROR)
    {
      __libdw_seterrno (dw_errno);
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return NULL;
    }
  return unwound;
}

/* Check if PC is in the "_start" function which may have no FDE.
   It corresponds to the GDB get_prev_frame logic "inside entry func".  */

static void
no_fde (Dwarf_Addr pc, Dwfl_Module *mod, Dwarf_Addr bias)
{
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (mod->main.elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return;
    }
  if (pc < ehdr->e_entry + bias)
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return;
    }
  GElf_Sym entry_sym;
  /* "_start" is size-less.  Search for PC, if the closest symbol is the one
     for E_ENTRY it belongs into the function starting at E_ENTRY.  */
  if (dwfl_module_addrsym (mod, pc - bias, &entry_sym, NULL) == NULL
      || entry_sym.st_value != ehdr->e_entry
      || (entry_sym.st_size != 0
	  && pc >= entry_sym.st_value + entry_sym.st_size + bias))
    {
      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
      return;
    }
  __libdwfl_seterrno (DWFL_E_RA_UNDEFINED);
}

static Dwarf_Frame_State *
handle_cfi (Dwarf_Frame_State *state, Dwarf_Addr pc, Dwfl_Module *mod, Dwarf_CFI *cfi, Dwarf_Addr bias)
{
  Dwarf_Frame *frame;
  if (dwarf_cfi_addrframe (cfi, pc, &frame) != 0)
    {
      int dw_errno = dwarf_errno ();
      if (dw_errno == DWARF_E_NO_MATCH)
	no_fde (pc, mod, bias);
      else
	{
	  __libdw_seterrno (dw_errno);
	  __libdwfl_seterrno (DWFL_E_LIBDW);
	}
      return NULL;
    }
  Dwarf_Frame_State *unwound = malloc (sizeof (*unwound) + sizeof (*unwound->regs) * state->base->nregs);
  state->unwound = unwound;
  unwound->base = state->base;
  unwound->unwound = NULL;
  unwound->regs_set = 0;
  for (unsigned regno = 0; regno < unwound->base->nregs; regno++)
    {
      Dwarf_Op reg_ops_mem[3], *reg_ops;
      size_t reg_nops;
      if (dwarf_frame_register (frame, regno, reg_ops_mem, &reg_ops, &reg_nops) != 0)
	{
	  __libdwfl_seterrno (DWFL_E_LIBDW);
	  return NULL;
	}
      Dwarf_Addr regval;
      if (reg_nops == 0)
	{
	  if (reg_ops == reg_ops_mem)
	    {
	      /* REGNO is undefined.  */
	      continue;
	    }
	  else if (reg_ops == NULL)
	    {
	      /* REGNO is same-value.  */
	      if (! state_get_reg (state, regno, &regval))
		return NULL;
	    }
	  else
	    {
	      __libdwfl_seterrno (DWFL_E_UNKNOWN_ERROR);
	      return NULL;
	    }
	}
      else if (! expr_eval (state, frame, reg_ops, reg_nops, &regval))
	return NULL;
      unwound->regs[regno] = regval;
      unwound->regs_set |= 1U << regno;
    }
  return have_unwound (unwound);
}

Dwarf_Frame_State *
dwfl_frame_unwind (Dwarf_Frame_State *state)
{
  if (state->unwound != NULL)
    return have_unwound (state->unwound);
  Dwarf_Addr pc = dwarf_frame_state_pc (state);
  int dw_errno = dwarf_errno ();
  if (dw_errno != DWARF_E_NOERROR)
    {
      __libdw_seterrno (dw_errno);
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return NULL;
    }
  Dwfl_Module *mod = dwfl_addrmodule (state->base->dwfl, pc);
  if (mod == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NO_DWARF);
      return NULL;
    }
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
