/* Test program for run-cfaloop.sh.
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

// gcc -o testfilecfaloop.S testfilecfaloop.c -Wall -O2 -dA -fno-dwarf2-cfi-asm -nostdlib -S
void _start(void) {
  volatile int *volatile p = 0;
  *p = 0;
}
