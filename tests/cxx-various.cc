/* Test program for testing C++11 features are recognized.
   Copyright (C) 2012 Red Hat, Inc.
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

// NOTE this file is the source of the binary testfile-cxx-various created
// with g++ -std=c++0x -g -o testfile-cxx-various cxx-various.cc
//
// When you edit this file to add more C++ contructs either create
// a new source file, or regenerate the binary test file.  We always
// want to include the source code of all our test binaries.

#include <iostream>
using namespace std;

// Make sure we recognize the encoding of some of the standard types.
// char16_t and char_32_t use DW_ATE_UTF.
char c;
signed char sc;
unsigned char uc;
int i;
float f;
double d;
wchar_t wc;
char16_t c16;
char32_t c32;
long l;

long
standard_types_func (char ac, signed char asc, unsigned char auc,
		     int ai, float af, double ad, wchar_t awc,
		     char16_t ac16, char32_t ac32, long al)
{
  c = ac;
  sc = asc;
  uc = auc;
  i = ai;
  f = af;
  d = ad;
  wc = awc;
  c16 = ac16;
  c32 = ac32;
  l = al;
  return ac + asc + auc + ai + af + ad + awc + ac16 + ac32 + al;
}

int main ()
{
  long answer;
  answer = standard_types_func (1, 2, 3, 4, 5.0, 6.0, 7, 8, 9, -10L);
  cout << "answer: " << answer << endl;
  return 0;
}
