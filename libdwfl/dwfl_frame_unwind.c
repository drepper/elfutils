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

#include "cfi.h"
#include <stdlib.h>
#include "libdwflP.h"
#include "../libdw/dwarf.h"
#include <sys/ptrace.h>

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static bool
state_get_reg (Dwfl_Frame_State *state, unsigned regno, Dwarf_Addr *val)
{
  if (! dwfl_frame_state_reg_get (state, regno, val))
    {
      __libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
      return false;
    }
  return true;
}

static int
bra_compar (const void *key_voidp, const void *elem_voidp)
{
  Dwarf_Word offset = (uintptr_t) key_voidp;
  const Dwarf_Op *op = elem_voidp;
  return (offset > op->offset) - (offset < op->offset);
}

/* FIXME: Handle bytecode deadlocks and overflows.  */

static bool
expr_eval (Dwfl_Frame_State *state, Dwarf_Frame *frame, const Dwarf_Op *ops,
	   size_t nops, Dwarf_Addr *result)
{
  Dwfl_Frame_State_Process *process = state->thread->process;
  if (nops == 0)
    {
      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
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
	    __libdwfl_seterrno (DWFL_E_NOMEM);
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
	__libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	return false;
      }
    *val = stack[--stack_used];
    return true;
  }
  Dwarf_Addr val1, val2;
  bool is_location = false;
  for (const Dwarf_Op *op = ops; op < ops + nops; op++)
    switch (op->atom)
    {
      case DW_OP_reg0 ... DW_OP_reg31:
	if (! state_get_reg (state, op->atom - DW_OP_reg0, &val1)
	    || ! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_regx:
	if (! state_get_reg (state, op->number, &val1) || ! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_breg0 ... DW_OP_breg31:
	if (! state_get_reg (state, op->atom - DW_OP_breg0, &val1))
	  {
	    free (stack);
	    return false;
	  }
	val1 += op->number;
	if (! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_bregx:
	if (! state_get_reg (state, op->number, &val1))
	  {
	    free (stack);
	    return false;
	  }
	val1 += op->number2;
	if (! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_lit0 ... DW_OP_lit31:
	if (! push (op->atom - DW_OP_lit0))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_plus_uconst:
	if (! pop (&val1) || ! push (val1 + op->number))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_call_frame_cfa:;
	Dwarf_Op *cfa_ops;
	size_t cfa_nops;
	Dwarf_Addr cfa;
	if (dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops) != 0
	    || ! expr_eval (state, frame, cfa_ops, cfa_nops, &cfa)
	    || ! push (cfa))
	  {
	    __libdwfl_seterrno (DWFL_E_LIBDW);
	    free (stack);
	    return false;
	  }
	is_location = true;
	break;
      case DW_OP_stack_value:
	is_location = false;
	break;
      case DW_OP_deref:
	if (! pop (&val1)
	    || ! process->memory_read (val1, &val1,
				       process->memory_read_user_data)
	    || ! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_nop:
	break;
      case DW_OP_dup:
	if (! pop (&val1) || ! push (val1) || ! push (val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_const1u:
      case DW_OP_const1s:
      case DW_OP_const2u:
      case DW_OP_const2s:
      case DW_OP_const4u:
      case DW_OP_const4s:
      case DW_OP_const8u:
      case DW_OP_const8s:
      case DW_OP_constu:
      case DW_OP_consts:
	if (! push (op->number))
	  {
	    free (stack);
	    return false;
	  }
	break;
      case DW_OP_bra:
	if (! pop (&val1))
	  {
	    free (stack);
	    return false;
	  }
	if (val1 == 0)
	  break;
	/* FALLTHRU */
      case DW_OP_skip:;
	Dwarf_Word offset = op->offset + 1 + 2 + (int16_t) op->number;
	const Dwarf_Op *found = bsearch ((void *) (uintptr_t) offset, ops, nops,
					 sizeof (*ops), bra_compar);
	if (found == NULL)
	  {
	    free (stack);
	    /* PPC32 vDSO has such invalid operations.  */
	    __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	    return false;
	  }
	/* Undo the 'for' statement increment.  */
	op = found - 1;
	break;
      case DW_OP_drop:
	if (! pop (&val1))
	  {
	    free (stack);
	    return false;
	  }
	break;
#define BINOP(atom, op)							\
      case atom:							\
	if (! pop (&val2) || ! pop (&val1) || ! push (val1 op val2))	\
	  {								\
	    free (stack);						\
	    return false;						\
	  }								\
	break;
      BINOP (DW_OP_and, &)
      BINOP (DW_OP_shl, <<)
      BINOP (DW_OP_plus, +)
      BINOP (DW_OP_mul, *)
#undef BINOP
#define BINOP_SIGNED(atom, op)						\
      case atom:							\
	if (! pop (&val2) || ! pop (&val1)				\
	    || ! push ((int64_t) val1 op (int64_t) val2))		\
	  {								\
	    free (stack);						\
	    return false;						\
	  }								\
	break;
      BINOP_SIGNED (DW_OP_le, <=)
      BINOP_SIGNED (DW_OP_ge, >=)
      BINOP_SIGNED (DW_OP_eq, ==)
      BINOP_SIGNED (DW_OP_lt, <)
      BINOP_SIGNED (DW_OP_gt, >)
      BINOP_SIGNED (DW_OP_ne, !=)
#undef BINOP_SIGNED
      default:
	__libdwfl_seterrno (DWFL_E_UNSUPPORTED_DWARF);
	return false;
    }
  if (! pop (result))
    {
      free (stack);
      return false;
    }
  free (stack);
  if (is_location && ! process->memory_read (*result, result,
					     process->memory_read_user_data))
    return false;
  return true;
}

/* Return TRUE and update *STATEP for the unwound frame for successful unwind.
   Return TRUE and set *STATEP to NULL for the outermost frame.  Return FALSE
   (and call __libdwfl_seterrno) otherwise.  */

static bool
have_unwound (Dwfl_Frame_State **statep)
{
  Dwfl_Frame_State *state = *statep, *unwound = state->unwound;
  switch (unwound->pc_state)
  {
    case DWFL_FRAME_STATE_ERROR:
      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
      *statep = NULL;
      return false;
    case DWFL_FRAME_STATE_PC_SET:
      *statep = unwound;
      return true;
    case DWFL_FRAME_STATE_PC_UNDEFINED:
      *statep = NULL;
      return true;
  }
  abort ();
}

/* Check if PC is in the "_start" function which may have no FDE.
   It corresponds to the GDB get_prev_frame logic "inside entry func".
   Return TRUE if PC is in an outer frame.  Return FALSE (and call
   __libdwfl_seterrno) otherwise.  */

static bool
no_fde (Dwarf_Addr pc, Dwfl_Module *mod, Dwarf_Addr bias)
{
  GElf_Sym sym;
  const char *symname = INTUSE(dwfl_module_addrsym) (mod, pc, &sym, NULL);
  if (symname == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NO_DWARF);
      return false;
    }
  /* It has no FDE on PPC64; it can be still unwound via the stack frame.  */
  if (strcmp (symname, ".generic_start_main") == 0)
    return true;
  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (mod->main.elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return false;
    }
  if (pc < ehdr->e_entry + bias)
    {
      __libdwfl_seterrno (DWFL_E_NO_DWARF);
      return false;
    }
  /* "_start" is size-less.  Search for PC, if the closest symbol is the one
     for E_ENTRY it belongs into the function starting at E_ENTRY.  */
  if (sym.st_value != ehdr->e_entry + bias
      || (sym.st_size != 0 && pc >= sym.st_value + sym.st_size))
    {
      __libdwfl_seterrno (DWFL_E_NO_DWARF);
      return false;
    }
  return true;
}

/* The logic is to call __libdwfl_seterrno for any CFI bytecode interpretation
   error so one can easily catch the problem with a debugger.  Still there are
   archs with invalid CFI for some registers where the registers are never used
   later.  Therefore we continue unwinding leaving the registers undefined.

   The only exception is PC itself, when there is an error unwinding PC we
   return false.  Otherwise we would return successful end of backtrace seeing
   an undefined PC register (due to an error unwinding it).  */

static bool
handle_cfi (Dwfl_Frame_State **statep, Dwarf_Addr pc, Dwfl_Module *mod,
	    Dwarf_CFI *cfi, Dwarf_Addr bias)
{
  Dwfl_Frame_State *state = *statep;
  Dwarf_Frame *frame;
  if (INTUSE(dwarf_cfi_addrframe) (cfi, pc - bias, &frame) != 0)
    {
      int dw_errno = dwarf_errno ();
      if (dw_errno == DWARF_E_NO_MATCH)
	{
	  if (! no_fde (pc, mod, bias))
	    return false;
	  *statep = NULL;
	  return true;
	}
      __libdw_seterrno (dw_errno);
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return false;
    }
  Dwfl_Frame_State_Thread *thread = state->thread;
  Dwfl_Frame_State_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  size_t nregs = ebl_frame_state_nregs (ebl);
  Dwfl_Frame_State *unwound;
  unwound = malloc (sizeof (*unwound) + sizeof (*unwound->regs) * nregs);
  state->unwound = unwound;
  unwound->thread = thread;
  unwound->unwound = NULL;
  unwound->signal_frame = frame->fde->cie->signal_frame;
  unwound->pc_state = DWFL_FRAME_STATE_ERROR;
  memset (unwound->regs_set, 0, sizeof (unwound->regs_set));
  for (unsigned regno = 0; regno < nregs; regno++)
    {
      Dwarf_Op reg_ops_mem[3], *reg_ops;
      size_t reg_nops;
      if (dwarf_frame_register (frame, regno, reg_ops_mem, &reg_ops,
				&reg_nops) != 0)
	{
	  __libdwfl_seterrno (DWFL_E_LIBDW);
	  continue;
	}
      Dwarf_Addr regval;
      if (reg_nops == 0)
	{
	  if (reg_ops == reg_ops_mem)
	    {
	      /* REGNO is undefined.  */
	      unsigned ra = frame->fde->cie->return_address_register;
	      if (ebl_frame_dwarf_to_regno (ebl, &ra) && regno == ra)
		unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
	      continue;
	    }
	  else if (reg_ops == NULL)
	    {
	      /* REGNO is same-value.  */
	      if (! state_get_reg (state, regno, &regval))
		continue;
	    }
	  else
	    {
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      continue;
	    }
	}
      else if (! expr_eval (state, frame, reg_ops, reg_nops, &regval))
	{
	  /* PPC32 vDSO has various invalid operations, ignore them.  The
	     register will look as unset causing an error later, if used.
	     But PPC32 does not use such registers.  */
	  continue;
	}
      if (! dwfl_frame_state_reg_set (unwound, regno, regval))
	{
	  __libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
	  continue;
	}
    }
  if (unwound->pc_state == DWFL_FRAME_STATE_ERROR
      && dwfl_frame_state_reg_get (unwound,
				   frame->fde->cie->return_address_register,
				   &unwound->pc))
    {
      /* PPC32 __libc_start_main properly CFI-unwinds PC as zero.  Currently
	 none of the archs supported for unwinding have zero as a valid PC.  */
      if (unwound->pc == 0)
	unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
      else
	unwound->pc_state = DWFL_FRAME_STATE_PC_SET;
    }
  return have_unwound (statep);
}

bool
dwfl_frame_unwind (Dwfl_Frame_State **statep)
{
  Dwfl_Frame_State *state = *statep;
  if (state->unwound)
    return have_unwound (statep);
  Dwarf_Addr pc;
  bool ok = INTUSE(dwfl_frame_state_pc) (state, &pc, NULL);
  assert (ok);
  /* Do not ask for MINUSONE dwfl_frame_state_pc, it would try to unwind STATE
     which would deadlock us.  */
  if (state != state->thread->unwound && ! state->signal_frame)
    pc--;
  Dwfl_Module *mod = INTUSE(dwfl_addrmodule) (state->thread->process->dwfl, pc);
  if (mod != NULL)
    {
      Dwarf_Addr bias;
      Dwarf_CFI *cfi_eh = INTUSE(dwfl_module_eh_cfi) (mod, &bias);
      if (cfi_eh)
	{
	  if (handle_cfi (statep, pc, mod, cfi_eh, bias))
	    return true;
	  if (state->unwound)
	    {
	      assert (state->unwound->pc_state == DWFL_FRAME_STATE_ERROR);
	      return false;
	    }
	}
      Dwarf_CFI *cfi_dwarf = INTUSE(dwfl_module_dwarf_cfi) (mod, &bias);
      if (cfi_dwarf)
	{
	  if (handle_cfi (statep, pc, mod, cfi_dwarf, bias) && state->unwound)
	    return true;
	  if (state->unwound)
	    {
	      assert (state->unwound->pc_state == DWFL_FRAME_STATE_ERROR);
	      return false;
	    }
	}
    }
  *statep = state;
  if (ebl_frame_unwind (state->thread->process->ebl, statep, pc))
    return true;
  if (state->unwound)
    {
      assert (state->unwound->pc_state == DWFL_FRAME_STATE_ERROR);
      return false;
    }
  __libdwfl_seterrno (DWFL_E_NO_DWARF);
  return false;
}
INTDEF(dwfl_frame_unwind)
