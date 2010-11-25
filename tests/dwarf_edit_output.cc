/* Test program for dwarf_edit, dwarf_output transforms with dwarf_comparator.
   Copyright (C) 2010 Red Hat, Inc.
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

#include "error.h"

#include "c++/dwarf"
#include "c++/dwarf_edit"
#include "c++/dwarf_output"
#include "c++/dwarf_comparator"
#include "c++/dwarf_tracker"

#include "c++/subr.hh"

#include "../src/dwarfstrings.h"

using namespace elfutils;
using namespace std;

// Only used for testing.
#include "print-die.hh"

typedef dwarf_ref_tracker<dwarf_edit, dwarf_edit> cmp_tracker;
struct cmp
  : public dwarf_comparator<dwarf_edit, dwarf_edit, false, cmp_tracker>
{
  cmp_tracker _m_tracker;

  cmp ()
    : dwarf_comparator<dwarf_edit, dwarf_edit, false, cmp_tracker> (_m_tracker)
  {}

  bool operator () (const dwarf_edit &a, const dwarf_edit &b)
  {
    return equals (a, b);
  }

  // Customized compare function. Takes two debug_info_entries and
  // the dwarf structure they reside in. The first debug_info_entry is assumed
  // to reside in the first cu, the debug_info_entry in the next cu.
  bool
  compare_dies (const dwarf_edit::debug_info_entry &a,
		const dwarf_edit::debug_info_entry &b,
		const dwarf_edit &dw)
  {
    dwarf_edit::compile_units_type::const_iterator cu1, cu2;
    cu1 = dw.compile_units ().begin ();
    cu2 = dw.compile_units ().begin ();
    cu2++;

    cmp_tracker::walk in (&this->_m_tracker, cu1, cu2);

    in.jump (a, b);
    return equals (a, b);
  }

  bool
  compare_first_two_cus (const dwarf_edit &dw)
  {
    dwarf_edit::compile_units_type::const_iterator cu1, cu2;
    cu1 = dw.compile_units ().begin ();
    cu2 = dw.compile_units ().begin ();
    cu2++;

    cmp_tracker::walk in (&this->_m_tracker, cu1, cu2);

    return equals (*cu1, *cu2);
  }
};

dwarf_edit &
empty_cu (dwarf_edit &in)
{
  in.add_unit ();
  return in;
}

dwarf_edit &
empty_cus (dwarf_edit &in)
{
  in.add_unit ();
  in.add_unit ();
  in.add_unit ();

  return in;
}

dwarf_edit &
two_same_dies (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu1 = in.add_unit ();
  cu1.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer attr1
    = cu1.add_entry (DW_TAG_base_type);
  attr1->attributes ()[DW_AT_name].identifier () = "int";
  // XXX Not a dwarf_constant? Prints out wrongly
  //attr1->attributes ()[DW_AT_encoding].dwarf_constant () = DW_ATE_signed;
  attr1->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::compile_unit &cu2 = in.add_unit ();
  cu2.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";
  dwarf_edit::debug_info_entry::pointer attr2
    = cu2.add_entry (DW_TAG_base_type);
  attr2->attributes ()[DW_AT_name].identifier () = "int";
  attr2->attributes ()[DW_AT_byte_size].constant () = 4;

  cmp compare;
  if (! compare.compare_dies (*attr1, *attr2, in))
    error (-1, 0, "two_same_dies not equal");

  return in;
}

dwarf_edit &
var_ref_type (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu = in.add_unit ();
  cu.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer type = cu.add_entry (DW_TAG_base_type);
  type->attributes ()[DW_AT_name].identifier () = "int";
  type->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry &var = *cu.add_entry (DW_TAG_variable);
  var.attributes ()[DW_AT_name].identifier () = "var";
  var.attributes ()[DW_AT_type].reference () = type;

  return in;
}

dwarf_edit &
var_ref_type_after (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu = in.add_unit ();
  cu.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry &var = *cu.add_entry (DW_TAG_variable);
  var.attributes ()[DW_AT_name].identifier () = "var";

  dwarf_edit::debug_info_entry::pointer type = cu.add_entry (DW_TAG_base_type);
  type->attributes ()[DW_AT_name].identifier () = "int";
  type->attributes ()[DW_AT_byte_size].constant () = 4;

  var.attributes ()[DW_AT_type].reference () = type;

  return in;
}

dwarf_edit &
dup_same_type_vars (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu1 = in.add_unit ();
  cu1.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer type1
    = cu1.add_entry (DW_TAG_base_type);
  type1->attributes ()[DW_AT_name].identifier () = "int";
  type1->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry &var1 = *cu1.add_entry (DW_TAG_variable);
  var1.attributes ()[DW_AT_name].identifier () = "var1";
  var1.attributes ()[DW_AT_type].reference () = type1;

  dwarf_edit::compile_unit &cu2 = in.add_unit ();
  cu2.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer type2
    = cu2.add_entry (DW_TAG_base_type);
  type2->attributes ()[DW_AT_name].identifier () = "int";
  type2->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry &var2 = *cu2.add_entry (DW_TAG_variable);
  var2.attributes ()[DW_AT_name].identifier () = "var2";
  var2.attributes ()[DW_AT_type].reference () = type2;

  // Types are equal.
  cmp compare;
  if (! compare.compare_dies (*type1, *type2, in))
    error (-1, 0, "dup_same_type_vars types not equal");

  // But vars have different names.
  cmp compare2;
  if (compare2.compare_dies (var1, var2, in))
    error (-1, 0, "two_same_type_vars vars equal");

  return in;
}

dwarf_edit &
circular_struct (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu = in.add_unit ();
  cu.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer int_ref
    = cu.add_entry (DW_TAG_base_type);
  int_ref->attributes ()[DW_AT_name].identifier () = "int";
  int_ref->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref
    = cu.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref->attributes ()[DW_AT_byte_size].constant () = 8;

  dwarf_edit::debug_info_entry::pointer list_ptr
    = cu.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list = *list_ptr;
  list.attributes ()[DW_AT_name].identifier () = "list";
  list.attributes ()[DW_AT_byte_size].constant () = 0x10;

  dwarf_edit::debug_info_entry &mi = *list.add_entry (DW_TAG_member);
  mi.attributes ()[DW_AT_name].identifier () = "i";
  mi.attributes ()[DW_AT_type].reference () = int_ref;

  dwarf_edit::debug_info_entry &mn = *list.add_entry (DW_TAG_member);
  mn.attributes ()[DW_AT_name].identifier () = "next";
  mn.attributes ()[DW_AT_type].reference () = struct_ptr_ref;

  struct_ptr_ref->attributes ()[DW_AT_type].reference () = list_ptr;

  dwarf_edit::debug_info_entry &var = *cu.add_entry (DW_TAG_variable);
  var.attributes ()[DW_AT_name].identifier () = "var";
  var.attributes ()[DW_AT_type].reference () = struct_ptr_ref;

  return in;
}

// Same as above, but with struct pointer type defined after struct.
dwarf_edit &
circular_struct2 (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu = in.add_unit ();
  cu.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer int_ref
    = cu.add_entry (DW_TAG_base_type);
  int_ref->attributes ()[DW_AT_name].identifier () = "int";
  int_ref->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry::pointer list_ptr
    = cu.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list = *list_ptr;
  list.attributes ()[DW_AT_name].identifier () = "list";
  list.attributes ()[DW_AT_byte_size].constant () = 0x10;

  dwarf_edit::debug_info_entry &mi = *list.add_entry (DW_TAG_member);
  mi.attributes ()[DW_AT_name].identifier () = "i";
  mi.attributes ()[DW_AT_type].reference () = int_ref;

  dwarf_edit::debug_info_entry &mn = *list.add_entry (DW_TAG_member);
  mn.attributes ()[DW_AT_name].identifier () = "next";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref
    = cu.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref->attributes ()[DW_AT_byte_size].constant () = 8;
  struct_ptr_ref->attributes ()[DW_AT_type].reference () = list_ptr;

  mn.attributes ()[DW_AT_type].reference () = struct_ptr_ref;

  dwarf_edit::debug_info_entry &var = *cu.add_entry (DW_TAG_variable);
  var.attributes ()[DW_AT_name].identifier () = "var";
  var.attributes ()[DW_AT_type].reference () = struct_ptr_ref;

  return in;
}

dwarf_edit &
two_circular_structs (dwarf_edit &in)
{
  circular_struct (in);
  circular_struct (in);

  return in;
}

dwarf_edit &
two_circular_structs2 (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu1 = in.add_unit ();
  cu1.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer int_ref1
    = cu1.add_entry (DW_TAG_base_type);
  int_ref1->attributes ()[DW_AT_name].identifier () = "int";
  int_ref1->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref1
    = cu1.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref1->attributes ()[DW_AT_byte_size].constant () = 8;

  dwarf_edit::debug_info_entry::pointer list_ptr1
    = cu1.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list1 = *list_ptr1;
  list1.attributes ()[DW_AT_name].identifier () = "list";
  list1.attributes ()[DW_AT_byte_size].constant () = 0x10;

  dwarf_edit::debug_info_entry &mi1 = *list1.add_entry (DW_TAG_member);
  mi1.attributes ()[DW_AT_name].identifier () = "i";
  mi1.attributes ()[DW_AT_type].reference () = int_ref1;

  dwarf_edit::debug_info_entry &mn1 = *list1.add_entry (DW_TAG_member);
  mn1.attributes ()[DW_AT_name].identifier () = "next";
  mn1.attributes ()[DW_AT_type].reference () = struct_ptr_ref1;

  struct_ptr_ref1->attributes ()[DW_AT_type].reference () = list_ptr1;

  dwarf_edit::debug_info_entry &var1 = *cu1.add_entry (DW_TAG_variable);
  var1.attributes ()[DW_AT_name].identifier () = "var";
  var1.attributes ()[DW_AT_type].reference () = struct_ptr_ref1;

  // Second CU

  dwarf_edit::compile_unit &cu2 = in.add_unit ();
  cu2.attributes ()[DW_AT_producer].string () = "dwarf_edit_output_test";

  dwarf_edit::debug_info_entry::pointer int_ref2
    = cu2.add_entry (DW_TAG_base_type);
  int_ref2->attributes ()[DW_AT_name].identifier () = "int";
  int_ref2->attributes ()[DW_AT_byte_size].constant () = 4;

  dwarf_edit::debug_info_entry::pointer list_ptr2
    = cu2.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list2 = *list_ptr2;
  list2.attributes ()[DW_AT_name].identifier () = "list";
  list2.attributes ()[DW_AT_byte_size].constant () = 0x10;

  dwarf_edit::debug_info_entry &mi2 = *list2.add_entry (DW_TAG_member);
  mi2.attributes ()[DW_AT_name].identifier () = "i";
  mi2.attributes ()[DW_AT_type].reference () = int_ref2;

  dwarf_edit::debug_info_entry &mn2 = *list2.add_entry (DW_TAG_member);
  mn2.attributes ()[DW_AT_name].identifier () = "next";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref2
    = cu2.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref2->attributes ()[DW_AT_byte_size].constant () = 8;
  struct_ptr_ref2->attributes ()[DW_AT_type].reference () = list_ptr2;

  mn2.attributes ()[DW_AT_type].reference () = struct_ptr_ref2;

  dwarf_edit::debug_info_entry &var2 = *cu2.add_entry (DW_TAG_variable);
  var2.attributes ()[DW_AT_name].identifier () = "var";
  var2.attributes ()[DW_AT_type].reference () = struct_ptr_ref2;

  return in;
}

dwarf_edit &
var_struct_ptr_type (dwarf_edit &in)
{
  /* A variable referencing the struct pointer type "directly",
     and through its "own" definition of the structure pointer type,
     should be considered equal. Some discussion in:
     https://fedorahosted.org/pipermail/elfutils-devel/2010-July/001480.html
  */

  // CU1
  dwarf_edit::compile_unit &cu1 = in.add_unit ();
  cu1.attributes ()[DW_AT_producer].string () = "CU";

  dwarf_edit::debug_info_entry::pointer list_ptr1
    = cu1.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list1 = *list_ptr1;
  list1.attributes ()[DW_AT_name].identifier () = "list";
  list1.attributes ()[DW_AT_byte_size].constant () = 12;

  dwarf_edit::debug_info_entry &mn1 = *list1.add_entry (DW_TAG_member);
  mn1.attributes ()[DW_AT_name].identifier () = "next";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref1
    = cu1.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref1->attributes ()[DW_AT_byte_size].constant () = 8;
  struct_ptr_ref1->attributes ()[DW_AT_type].reference () = list_ptr1;

  mn1.attributes ()[DW_AT_type].reference () = struct_ptr_ref1;

  dwarf_edit::debug_info_entry &var1 = *cu1.add_entry (DW_TAG_variable);
  var1.attributes ()[DW_AT_name].identifier () = "var";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref_b
    = cu1.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref_b->attributes ()[DW_AT_byte_size].constant () = 8;
  struct_ptr_ref_b->attributes ()[DW_AT_type].reference () = list_ptr1;

  var1.attributes ()[DW_AT_type].reference () = struct_ptr_ref_b;

  // CU2
  dwarf_edit::compile_unit &cu2 = in.add_unit ();
  cu2.attributes ()[DW_AT_producer].string () = "CU";

  dwarf_edit::debug_info_entry::pointer list_ptr2
    = cu2.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list2 = *list_ptr2;
  list2.attributes ()[DW_AT_name].identifier () = "list";
  list2.attributes ()[DW_AT_byte_size].constant () = 12;

  dwarf_edit::debug_info_entry &mn2 = *list2.add_entry (DW_TAG_member);
  mn2.attributes ()[DW_AT_name].identifier () = "next";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref2
    = cu2.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref2->attributes ()[DW_AT_byte_size].constant () = 8;
  struct_ptr_ref2->attributes ()[DW_AT_type].reference () = list_ptr2;

  mn2.attributes ()[DW_AT_type].reference () = struct_ptr_ref2;

  dwarf_edit::debug_info_entry &var2 = *cu2.add_entry (DW_TAG_variable);
  var2.attributes ()[DW_AT_name].identifier () = "var";
  var2.attributes ()[DW_AT_type].reference () = struct_ptr_ref2;

  cmp compare;
  if (! compare.compare_dies (var1, var2, in))
    error (-1, 0, "var_struct_ptr_type vars not equal");

  return in;
}

dwarf_edit &
small_circular_structs (dwarf_edit &in)
{
  dwarf_edit::compile_unit &cu1 = in.add_unit ();
  dwarf_edit::debug_info_entry::pointer struct_ptr_ref1
    = cu1.add_entry (DW_TAG_pointer_type);

  dwarf_edit::debug_info_entry::pointer list_ptr1
    = cu1.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list1 = *list_ptr1;
  list1.attributes ()[DW_AT_name].identifier () = "list";

  dwarf_edit::debug_info_entry &mn1 = *list1.add_entry (DW_TAG_member);
  mn1.attributes ()[DW_AT_name].identifier () = "next";
  mn1.attributes ()[DW_AT_type].reference () = struct_ptr_ref1;

  struct_ptr_ref1->attributes ()[DW_AT_type].reference () = list_ptr1;

  dwarf_edit::compile_unit &cu2 = in.add_unit ();
  dwarf_edit::debug_info_entry::pointer list_ptr2
    = cu2.add_entry (DW_TAG_structure_type);
  dwarf_edit::debug_info_entry &list2 = *list_ptr2;
  list2.attributes ()[DW_AT_name].identifier () = "list";

  dwarf_edit::debug_info_entry &mn2 = *list2.add_entry (DW_TAG_member);
  mn2.attributes ()[DW_AT_name].identifier () = "next";

  dwarf_edit::debug_info_entry::pointer struct_ptr_ref2
    = cu2.add_entry (DW_TAG_pointer_type);
  struct_ptr_ref2->attributes ()[DW_AT_type].reference () = list_ptr2;

  mn2.attributes ()[DW_AT_type].reference () = struct_ptr_ref2;

  return in;
}

static int show_input, show_output;

/* Tests whether the last die in the first CU and the last die in the
   second CU with the same tag have the same offset (which means they
   were/can be merged). Succeeds when the comparison matches 'same'
   argument (or the 'tag' couldn't be found in both CUs). */
void
test_last_two_dies (dwarf_edit &in, dwarf_output &out, int tag, bool same,
		    int n, const char *name)
{
  dwarf_edit::compile_units_type::const_iterator cu_in;
  dwarf_edit::debug_info_entry::children_type::const_iterator it_in;
  const dwarf_edit::debug_info_entry *die1 = NULL;
  const dwarf_edit::debug_info_entry *die2 = NULL;

  cu_in = in.compile_units ().begin ();
  it_in = (*cu_in).children ().begin ();
  while (it_in != (*cu_in).children ().end ())
    {
      if ((*it_in).tag () == tag)
	die1 = &(*it_in);
      it_in++;
    }
  if (show_input)
    cout << "input cu1 last: " << (die1 ? (*die1).to_string () : "NULL")
	 << endl;

  cu_in++;
  it_in = (*cu_in).children ().begin ();
  while (it_in != (*cu_in).children ().end ())
    {
      if ((*it_in).tag () == tag)
	die2 = &(*it_in);
      it_in++;
    }
  if (show_input)
    cout << "input cu2 last: " << (die2 ? (*die2).to_string () : "NULL")
	 << endl;

  if (die1 != NULL)
    {
      cmp compare;
      if (compare.compare_dies (*die1, *die2, in) != same)
	error (-1, 0, "dwarf_comparator fail %s test #%d '%s'",
	       dwarf_tag_string (tag), n, name);
    }

  dwarf_output::compile_units_type::const_iterator cu;
  dwarf_output::debug_info_entry::children_type::const_iterator it;
  ::Dwarf_Off off1 = 0;
  ::Dwarf_Off off2 = 0;

  cu = out.compile_units ().begin ();
  it = (*cu).children ().begin ();
  while (it != (*cu).children ().end ())
    {
      if ((*it).tag () == tag)
	off1 = (*it).offset ();
      it++;
    }
  if (show_output)
    cout << "offset last (" << dwarf_tag_string (tag) << ") cu1: "
	 << hex << off1 << endl;

  cu++;
  it = (*cu).children ().begin ();
  while (it != (*cu).children ().end ())
    {
      if ((*it).tag () == tag)
	off2 = (*it).offset ();
      it++;
    }
  if (show_output)
    cout << "offset last (" << dwarf_tag_string (tag) << ") cu2: "
	 << hex << off2 << endl;

  bool both_zero = off1 == 0 && off2 == 0;
  bool equal = off1 == off2;
  if (! both_zero && equal != same)
    error (-1, 0, "dwarf_comparator fail %s test #%d '%s'",
	   dwarf_tag_string (tag), n, name);

}

typedef dwarf_output::debug_info_entry::children_type::const_iterator ci;
struct match_offset : public std::binary_function<ci, ci, bool>
{
  inline bool operator () (const ci &ci1, const ci &ci2)
  {
    return (*ci1).offset () == (*ci2).offset ();
  }
};


/* Tests whether the first two CUs have the same children (offset/identity)
   which means they can be completely merged. */
bool
test_first_two_cus (dwarf_output &out)
{
  dwarf_output::compile_units_type::const_iterator cu;
  cu = out.compile_units ().begin ();
  ci children1 = (*cu).children ().begin ();
  ci end1 = (*cu).children ().end();

  cu++;
  ci children2 = (*cu).children ().begin ();
  ci end2 = (*cu).children ().end();

  return subr::container_equal (children1, end1, children2, end2,
				match_offset ());
}

void
test_run (int n, const char *name, dwarf_edit &in,
	  bool test_last, bool same_offset,
	  bool test_cus, bool same_cus)
{
  if (show_input | show_output)
    printf("*%s*\n", name);

  if (show_input)
    print_file ("dwarf_edit", in, 0);

  dwarf_output_collector c;
  dwarf_output out (in, c);

  if (show_output)
    {
      c.stats();
      print_file ("dwarf_output", out, 0);
    }

  // NOTE: dwarf_comparator ignore_refs = true
  dwarf_ref_tracker<dwarf_edit, dwarf_output> tracker;
  dwarf_comparator<dwarf_edit, dwarf_output, true> comp (tracker);
  if (! comp.equals (in, out))
    error (-1, 0, "fail test #%d '%s'", n, name);

  if (test_last)
    {
      test_last_two_dies (in, out, DW_TAG_variable,
			  same_offset, n, name);
      test_last_two_dies (in, out, DW_TAG_pointer_type,
			  same_offset, n, name);
      test_last_two_dies (in, out, DW_TAG_structure_type,
			  same_offset, n, name);
    }

  if (test_cus)
    {
      // test dwarf_comparator
      cmp compare;
      if (compare.compare_first_two_cus (in) != same_cus)
	error (-1, 0, "fail compare_first_two_cus #%d '%s'", n, name);

      // test dwarf_output
      if (test_first_two_cus (out) != same_cus)
	error (-1, 0, "fail test_first_two_cus #%d '%s'", n, name);
    }
}

int
main (int argc, char **argv)
{
  // Test number to run (or all if none given)
  int r = 0;
  if (argc > 1)
    {
      r = atoi(argv[1]);
      argc--;
      argv++;
    }

  // Whether to print input/output/both [in|out|inout]
  show_input = 0;
  show_output = 0;
  if (argc > 1)
    {
      if (strstr (argv[1], "in"))
	show_input = 1;
      if (strstr (argv[1], "out"))
	show_output = 1;
      argc--;
      argv++;
    }

  if (show_input | show_output)
    {
      // Abuse print_die_main initialization. Allows adding --offsets.
      // See print-die.cc for options one can add.
      unsigned int d;
      print_die_main (argc, argv, d);
    }

#define RUNTEST(N) (r == 0 || r == N)

  dwarf_edit in1;
  if (RUNTEST (1))
    test_run (1, "empty_cu", empty_cu(in1), false, false, false, false);

  dwarf_edit in2;
  if (RUNTEST (2))
    test_run (2, "empty_cus", empty_cus(in2), false, false, true, true);

  dwarf_edit in3;
  if (RUNTEST (3))
    test_run (3, "two_same_dies", two_same_dies (in3),
	      false, false, true, true);

  dwarf_edit in4;
  if (RUNTEST (4))
    test_run (4, "var_ref_type", var_ref_type (in4),
	      false, false, false, false);

  dwarf_edit in5;
  if (RUNTEST (5))
    test_run (5, "var_ref_type_after", var_ref_type_after (in5),
	      false, false, false, false);

  dwarf_edit in6;
  if (RUNTEST (6))
    test_run (6, "dup_same_type_vars", dup_same_type_vars (in6),
	      true, false, true, false);

  dwarf_edit in7;
  if (RUNTEST (7))
    test_run (7, "circular_struct", circular_struct (in7),
	      false, false, false, false);

  dwarf_edit in8;
  if (RUNTEST (8))
    test_run (8, "circular_struct2", circular_struct2 (in8),
	      false, false, false, false);

  dwarf_edit in9;
  if (RUNTEST (9))
    test_run (9, "two_circular_structs", two_circular_structs (in9),
	      true, true, true, true);

  // Won't merge CUs since order of children different.
  // XXX vars are considered equal according to dwarf_comparator,
  // but not according to dwarf_output. Why not?
  dwarf_edit in10;
  if (RUNTEST (10))
    test_run (10, "two_circular_structs2", two_circular_structs2 (in10),
	      true, true, true, false);

  dwarf_edit in11;
  if (RUNTEST (11))
    test_run (11, "var_struct_ptr_type", var_struct_ptr_type (in11),
	      true, true, true, false);

  // Smallest example of issue with test 10.
  dwarf_edit in12;
  if (RUNTEST (12))
    test_run (12, "small_circular_structs", small_circular_structs (in12),
	      true, true, true, false);

  return 0;
}
