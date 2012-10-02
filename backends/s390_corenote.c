/* S390 specific core note handling.
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

#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>

#ifndef BITS
# define BITS 		32
# define BACKEND	s390_
#else
# define BITS 		64
# define BACKEND	s390x_
#endif
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg)						\
    { .offset = at * BITS/8, .regno = dwreg, .count = n, .bits = BITS }

    GR ( 0,  1, 64),		/* pswm */
    GR ( 1,  1, 65),		/* pswa */
    GR ( 2, 16, 0),		/* r0-r15 */
    { .offset = 18 * BITS/8, .regno = 48, .count = 16, .bits = 32 } /* ar0-r15 */
    /* ar15 end is at (BITS == 32 ? 18 + 16 == 34 : 18 + 16 / 2 == 26).  */
    /* orig_r2 is at (BITS == 32 ? 34 : 26). */

#undef	GR
  };
#define PRSTATUS_REGS_SIZE	(BITS / 8 * (BITS == 32 ? 37 : 27))

static const Ebl_Register_Location fpregset_regs[] =
  {
#define FPR(at, n, dwreg)						\
    { .offset = at * 64/8, .regno = dwreg, .count = n, .bits = 64 }

    /* fpc is at 0.  */
    FPR (1 +  0, 1, 16),	/* f0 */
    FPR (1 +  1, 1, 20),	/* f1 */
    FPR (1 +  2, 1, 17),	/* f2 */
    FPR (1 +  3, 1, 21),	/* f3 */
    FPR (1 +  4, 1, 18),	/* f4 */
    FPR (1 +  5, 1, 22),	/* f5 */
    FPR (1 +  6, 1, 19),	/* f6 */
    FPR (1 +  7, 1, 23),	/* f7 */
    FPR (1 +  8, 1, 24),	/* f8 */
    FPR (1 +  9, 1, 28),	/* f9 */
    FPR (1 + 10, 1, 25),	/* f10 */
    FPR (1 + 11, 1, 29),	/* f11 */
    FPR (1 + 12, 1, 26),	/* f12 */
    FPR (1 + 13, 1, 30),	/* f13 */
    FPR (1 + 14, 1, 27),	/* f14 */
    FPR (1 + 15, 1, 31),	/* f15 */

#undef	FPR
  };
#define FPREGSET_SIZE		(17 * 8)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define TYPE_LONG		ELF_T_SWORD
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define TYPE_LONG		ELF_T_SXWORD
#endif
#define PID_T			int32_t
#define	UID_T			uint16_t
#define	GID_T			uint16_t
#define ALIGN_PID_T		4
#define ALIGN_UID_T		2
#define ALIGN_GID_T		2
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_WORD
#define TYPE_GID_T		ELF_T_WORD

#define PRSTATUS_REGSET_ITEMS					\
  {								\
    .name = "orig_r2", .type = TYPE_LONG, .format = 'd',	\
    .offset = offsetof (struct EBLHOOK(prstatus),		\
    pr_reg[BITS == 32 ? 34 : 26]), .group = "register"		\
  }

#if BITS == 32

static const Ebl_Register_Location high_regs[] =
  {
    /* Upper halves of r-r15 are stored here.  */
    /* FIXME: readelf -n does not display the values merged.  */
    { .offset = 0, .regno = 0, .count = 16, .bits = 32, .shift = 32 },
  };

static const Ebl_Core_Item no_items[0];

#define	EXTRA_NOTES \
  EXTRA_REGSET_ITEMS (NT_S390_HIGH_GPRS, 16 * BITS / 8, high_regs, no_items)

#endif

#include "linux-core-note.c"
