/* HPPA specific core note handling.
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

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

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
# define BACKEND	parisc_
#else
# define BITS 		64
# define BACKEND	parisc64_
#endif
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
    { .offset = 0, .regno = 0, .count = 32, .bits = BITS },	/* r0-r31 */
    { .offset = 32 * 4, .regno = 33, .count = 8, .bits = BITS },/* sr0-sr7 */
    { .offset = 40 * 4, .regno = 50, .count = 2, .bits = BITS },/* iaoq[01] */
    { .offset = 42 * 4, .regno = 60, .count = 2, .bits = BITS },/* iasq[01] */
    { .offset = 44 * 4, .regno = 32, .count = 1, .bits = BITS },/* sar */
    { .offset = 45 * 4, .regno = 119, .count = 4, .bits = BITS },/* cr19-cr22 */
    { .offset = 49 * 4, .regno = 100, .count = 4, .bits = BITS },/* cr0 */
    { .offset = 50 * 4, .regno = 124, .count = 8, .bits = BITS },/* cr24-cr31 */
    { .offset = 58 * 4, .regno = 108, .count = 2, .bits = BITS },/* cr8-cr9 */
    { .offset = 60 * 4, .regno = 112, .count = 2, .bits = BITS },/* cr12-cr13 */
    { .offset = 62 * 4, .regno = 110, .count = 1, .bits = BITS },/* cr10 */
    { .offset = 63 * 4, .regno = 115, .count = 1, .bits = BITS },/* cr15 */
  };
#define PRSTATUS_REGS_SIZE	(64 * 4)

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 64, .count = 32, .bits = 64 }, /* fr0-fr31 */
  };
#define FPREGSET_SIZE		(32 * 8)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define TYPE_LONG		ELF_T_SWORD
# define UID_T			uint32_t
# define GID_T			uint32_t
# define ALIGN_UID_T		2
# define ALIGN_GID_T		2
# define TYPE_UID_T		ELF_T_HALF
# define TYPE_GID_T		ELF_T_HALF
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define TYPE_LONG		ELF_T_SXWORD
# define UID_T			uint32_t
# define GID_T			uint32_t
# define ALIGN_UID_T		4
# define ALIGN_GID_T		4
# define TYPE_UID_T		ELF_T_WORD
# define TYPE_GID_T		ELF_T_WORD
# define SUSECONDS_HALF		1
#endif
#define PID_T			int32_t
#define ALIGN_PID_T		4
#define TYPE_PID_T		ELF_T_SWORD

#include "linux-core-note.c"
