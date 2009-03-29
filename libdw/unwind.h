#ifndef _libdw_unwind_h		/* XXX */
#define _libdw_unwind_h 1

#include "libdw.h"

/* This describes one Common Information Entry read from a CFI section.
   Pointers here point into the DATA->d_buf block passed to dwarf_next_cfi.  */
typedef struct
{
  Dwarf_Off CIE_id;    /* Always CIE_ID in Dwarf_CIE structures. */
#define CIE_ID	((Dwarf_Off) -1l)

  Dwarf_Word code_alignment_factor;
  Dwarf_Sword data_alignment_factor;
  Dwarf_Word return_address_register;

  const char *augmentation;	/* Augmentation string.  */

  /* Augmentation data, might be NULL.  The size is correct only if
     we understood the augmentation string sufficiently.  */
  const uint8_t *augmentation_data;
  size_t augmentation_data_size;
  size_t fde_augmentation_data_size;

  /* Instruction stream describing initial state used by FDEs.  If
     we did not understand the whole augmentation string and it did
     not use 'z', then there might be more augmentation data here
     (and in FDEs) before the actual instructions.  */
  const uint8_t *initial_instructions;
  const uint8_t *initial_instructions_end;
} Dwarf_CIE;

/* This describes one Frame Description Entry read from a CFI section.
   Pointers here point into the DATA->d_buf block passed to dwarf_next_cfi.  */
typedef struct
{
  /* Section offset of CIE this FDE refers to.  This will never be
     CIE_ID in an FDE.  If this value is CIE_ID, this is actually a
     Dwarf_CIE structure.  */
  Dwarf_Off CIE_pointer;

  /* We can't really decode anything further without looking up the CIE
     and checking its augmentation string.  Here follows the encoded
     initial_location and address_range, then any augmentation data,
     then the instruction stream.  This FDE describes PC locations in
     the byte range [initial_location, initial_location+address_range).
     When the CIE augmentation string uses 'z', the augmentation data is
     a DW_FORM_block (self-sized).  Otherwise, when we understand the
     augmentation string completely, fde_augmentation_data_size gives
     the number of bytes of augmentation data before the instructions.  */
  const uint8_t *start;
  const uint8_t *end;
} Dwarf_FDE;

typedef union
{
  Dwarf_CIE cie;
  Dwarf_FDE fde;
} Dwarf_CFI_Entry;

#define dwarf_cfi_cie_p(entry)	((entry)->cie.CIE_id == CIE_ID)

/* Decode one DWARF CFI entry (CIE or FDE) from the raw section data.
   The E_IDENT from the originating ELF file indicates the address
   size and byte order used in the CFI section contained in DATA;
   EH_FRAME_P should be true for .eh_frame format and false for
   .debug_frame format.  OFFSET is the byte position in the section
   to start at; on return *NEXT_OFFSET is filled in with the byte
   position immediately after this entry.

   On success, returns 0 and fills in *ENTRY; use dwarf_cfi_cie_p to
   see whether ENTRY->cie or ENTRY->fde is valid.

   On errors, returns -1.  Some format errors will permit safely
   skipping to the next CFI entry though the current one is unusable.
   In that case, *NEXT_OFF will be updated before a -1 return.

   If there are no more CFI entries left in the section,
   returns 1 and sets *NEXT_OFFSET to (Dwarf_Off) -1.  */
extern int dwarf_next_cfi (const unsigned char e_ident[],
			   Elf_Data *data, bool eh_frame_p,
			   Dwarf_Off offset, Dwarf_Off *next_offset,
			   Dwarf_CFI_Entry *entry)
  __nonnull_attribute__ (1, 2, 5, 6);


/* Opaque type representing a frame state described by CFI.  */
typedef struct Dwarf_Frame_s Dwarf_Frame;

/* Opaque type representing a CFI section found in a DWARF or ELF file.  */
typedef struct Dwarf_CFI_s Dwarf_CFI;

/* Use the CFI in the DWARF .debug_frame or .eh_frame section.
   Returns NULL if there is no such section.
   The pointer returned can be used until dwarf_end is called on DWARF,
   and must not be passed to dwarf_cfi_end.
   Calling this more than once returns the same pointer.  */
extern Dwarf_CFI *dwarf_getcfi (Dwarf *dwarf);

/* Use the CFI in the ELF file's exception-handling data.
   Returns NULL if there is no such data.
   The pointer returned can be used until elf_end is called on ELF,
   and must be passed to dwarf_cfi_end before then.
   Calling this more than once allocates independent data structures.  */
extern Dwarf_CFI *dwarf_getcfi_elf (Elf *elf);

/* Release resources allocated by dwarf_getcfi_elf.  */
extern int dwarf_cfi_end (Dwarf_CFI *cache);


/* Compute what's known about a call frame when the PC is at ADDRESS.
   Returns 0 for success or -1 for errors.
   On success, *FRAME is a malloc'd pointer.  */
extern int dwarf_cfi_addrframe (Dwarf_CFI *cache,
				Dwarf_Addr address, Dwarf_Frame **frame)
  __nonnull_attribute__ (3);

/* Return the DWARF register number used in FRAME to denote
   the return address in FRAME's caller frame.  The remaining
   arguments can be non-null to fill in more information.

   Fill [*START, *END) with the PC range to which FRAME's information applies.
   Fill in *SIGNALP to indicate whether this is a signal-handling frame.
   If true, this is the implicit call frame that calls a signal handler.
   This frame's "caller" is actually the interrupted state, not a call;
   its return address is an exact PC, not a PC after a call instruction.  */
extern int dwarf_frame_info (Dwarf_Frame *frame,
			     Dwarf_Addr *start, Dwarf_Addr *end, bool *signalp);

/* Deliver a DWARF expression that yields the Canonical Frame Address at
   this frame state.  Returns -1 for errors, or the number of operations
   stored at *OPS.  That pointer can be used only as long as FRAME is alive
   and unchanged.  Returns zero if the CFA cannot be determined here.  */
extern int dwarf_frame_cfa (Dwarf_Frame *frame, Dwarf_Op **ops)
  __nonnull_attribute__ (2);

/* Deliver a DWARF expression that yields the location or value of
   DWARF register number REGNO in the state described by FRAME.

   Returns -1 for errors, 0 if REGNO has an accessible location,
   or 1 if REGNO has only a computable value.  Stores at *NOPS
   the number of operations in the array stored at *OPS.
   With return value 0, this is a DWARF location expression.
   With return value 1, this is a DWARF expression that computes the value.

   Return value 1 with *NOPS zero means CFI says the caller's REGNO is
   "undefined" here, i.e. it's call-clobbered and cannot be recovered.

   Return value 0 with *NOPS zero means CFI says the caller's REGNO is
   "same_value" here, i.e. this frame did not change it; ask the caller
   frame where to find it.

   For common simple expressions *OPS is OPS_MEM.  For arbitrary DWARF
   expressions in the CFI, *OPS is an internal pointer that can be used as
   long as the Dwarf_CFI used to create FRAME remains alive.  */
extern int dwarf_frame_register (Dwarf_Frame *frame, int regno,
				 Dwarf_Op ops_mem[2],
				 Dwarf_Op **ops, size_t *nops)
  __nonnull_attribute__ (3, 4, 5);


// XXX libdwfl front-end
#include "../libdwfl/libdwfl.h"


/* Find the CFI for this module.  Returns NULL if there is no CFI.
   On success, fills in *BIAS with the difference between addresses
   within the loaded module and those in the CFI referring to it.
   The pointer returned can be used until the module is cleaned up.
   Calling this more than once returns the same pointer.  */
extern Dwarf_CFI *dwfl_module_getcfi (Dwfl_Module *mod, Dwarf_Addr *bias);

// XXX needs module bias? for DW_OP_addr in exprs?
/* Compute what's known about a call frame when the PC is at ADDRESS.
   Returns 0 for success or -1 for errors.
   On success, *FRAME is a malloc'd pointer.  */
extern int dwfl_addrframe (Dwfl *dwfl, Dwarf_Addr address, Dwarf_Frame **frame)
  __nonnull_attribute__ (3);

#endif	/* XXX */
