/* Pedantic checking of DWARF files
   Copyright (C) 2009,2010 Red Hat, Inc.
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

// The tables here capture attribute/allowed forms depending on DWARF
// version.  Apart from standardized DWARF formats, e.g. DWARF3+GNU is
// a version in its own.

#include "tables.hh"
#include "tables.h"
#include "../libdw/dwarf.h"

#include <map>
#include <bitset>
#include <cassert>

namespace
{
  enum dw_class {
    cl_address,
    cl_block,
    cl_constant,
    cl_exprloc,
    cl_flag,
    cl_reference,
    cl_string,
    cl_loclistptr,
    cl_lineptr,
    cl_macptr,
    cl_rangelistptr,
    max_dw_class
  };

  typedef std::bitset <max_dw_class> dw_class_set;

  dw_class_set dw_classes (dw_class a = max_dw_class, dw_class b = max_dw_class,
			   dw_class c = max_dw_class, dw_class d = max_dw_class,
			   dw_class e = max_dw_class)
  {
    dw_class_set s;
#define ADD(V) if (V != max_dw_class) s[V] = true
    ADD (a);
    ADD (b);
    ADD (c);
    ADD (d);
    ADD (e);
#undef ADD
    return s;
  }

  struct dwarf_row {
    int val;
    dw_class_set classes;
  };

  class form_table
  {
    typedef std::set <form> _form_set_t;
    typedef std::map <dw_class, _form_set_t > _form_map_t;

    static _form_map_t init_class_forms (dwarf_row const forms[],
					 form_table const *parent)
    {
      _form_map_t class_forms;

      if (parent != NULL)
	{
	  std::set <form> overriden_forms;
	  for (size_t i = 0; forms[i].val != 0; ++i)
	    overriden_forms.insert (forms[i].val);

	  for (_form_map_t::const_iterator it = parent->m_class_forms.begin ();
	       it != parent->m_class_forms.end (); ++it)
	    {
	      std::set <form> new_forms = it->second;
	      for (_form_set_t::const_iterator ot = overriden_forms.begin ();
		   ot != overriden_forms.end (); ++ot)
		new_forms.erase (*ot);
	      class_forms.insert (std::make_pair (it->first, new_forms));
	    }
	}

      for (size_t i = 0; forms[i].val != 0; ++i)
	for (unsigned c = 0; c < forms[i].classes.size (); ++c)
	  class_forms[static_cast<dw_class> (c)].insert (forms[i].val);

      return class_forms;
    }

    static _form_set_t init_all_forms (_form_map_t const &class_forms)
    {
      _form_set_t all_forms;
      for (_form_map_t::const_iterator it = class_forms.begin ();
	   it != class_forms.end (); ++it)
	all_forms.insert (it->second.begin (), it->second.end ());
      return all_forms;
    }

    _form_map_t const m_class_forms;
    _form_set_t const m_all_forms;

  public:
    form_table (dwarf_row const forms[], form_table const *parent = NULL)
      : m_class_forms (init_class_forms (forms, parent))
      , m_all_forms (init_all_forms (m_class_forms))
    {}

    std::set <form> const &get (dw_class c) const
    {
      _form_map_t::const_iterator it = m_class_forms.find (c);
      assert (it != m_class_forms.end ());
      return it->second;
    }

    std::set <form> const &get_all () const
    {
      return m_all_forms;
    }
  };

  dwarf_row const dwarf_2_at_table[] = {
    {DW_AT_sibling,		dw_classes (cl_reference)},
    {DW_AT_location,		dw_classes (cl_block, cl_constant)},
    {DW_AT_name,		dw_classes (cl_string)},
    {DW_AT_ordering,		dw_classes (cl_constant)},
    {DW_AT_byte_size,		dw_classes (cl_constant)},
    {DW_AT_bit_offset,		dw_classes (cl_constant)},
    {DW_AT_bit_size,		dw_classes (cl_constant)},
    {DW_AT_stmt_list,		dw_classes (cl_constant)},
    {DW_AT_low_pc,		dw_classes (cl_address)},
    {DW_AT_high_pc,		dw_classes (cl_address)},
    {DW_AT_language,		dw_classes (cl_constant)},
    {DW_AT_discr,		dw_classes (cl_reference)},
    {DW_AT_discr_value,		dw_classes (cl_constant)},
    {DW_AT_visibility,		dw_classes (cl_constant)},
    {DW_AT_import,		dw_classes (cl_reference)},
    {DW_AT_string_length,	dw_classes (cl_block, cl_constant)},
    {DW_AT_common_reference,	dw_classes (cl_reference)},
    {DW_AT_comp_dir,		dw_classes (cl_string)},
    {DW_AT_const_value,		dw_classes (cl_string, cl_constant, cl_block)},
    {DW_AT_containing_type,	dw_classes (cl_reference)},
    {DW_AT_default_value,	dw_classes (cl_reference)},
    {DW_AT_inline,		dw_classes (cl_constant)},
    {DW_AT_is_optional,		dw_classes (cl_flag)},
    {DW_AT_lower_bound,		dw_classes (cl_constant, cl_reference)},
    {DW_AT_producer,		dw_classes (cl_string)},
    {DW_AT_prototyped,		dw_classes (cl_flag)},
    {DW_AT_return_addr,		dw_classes (cl_block, cl_constant)},
    {DW_AT_start_scope,		dw_classes (cl_constant)},
    {DW_AT_bit_stride,		dw_classes (cl_constant)},
    {DW_AT_upper_bound,		dw_classes (cl_constant, cl_reference)},
    {DW_AT_abstract_origin,	dw_classes (cl_constant)},
    {DW_AT_accessibility,	dw_classes (cl_reference)},
    {DW_AT_address_class,	dw_classes (cl_constant)},
    {DW_AT_artificial,		dw_classes (cl_flag)},
    {DW_AT_base_types,		dw_classes (cl_reference)},
    {DW_AT_calling_convention,	dw_classes (cl_constant)},
    {DW_AT_count,		dw_classes (cl_constant, cl_reference)},
    {DW_AT_data_member_location, dw_classes (cl_block, cl_reference)},
    {DW_AT_decl_column,		dw_classes (cl_constant)},
    {DW_AT_decl_file,		dw_classes (cl_constant)},
    {DW_AT_decl_line,		dw_classes (cl_constant)},
    {DW_AT_declaration,		dw_classes (cl_flag)},
    {DW_AT_discr_list,		dw_classes (cl_block)},
    {DW_AT_encoding,		dw_classes (cl_constant)},
    {DW_AT_external,		dw_classes (cl_flag)},
    {DW_AT_frame_base,		dw_classes (cl_block, cl_constant)},
    {DW_AT_friend,		dw_classes (cl_reference)},
    {DW_AT_identifier_case,	dw_classes (cl_constant)},
    {DW_AT_macro_info,		dw_classes (cl_constant)},
    {DW_AT_namelist_item,	dw_classes (cl_block)},
    {DW_AT_priority,		dw_classes (cl_reference)},
    {DW_AT_segment,		dw_classes (cl_block, cl_constant)},
    {DW_AT_specification,	dw_classes (cl_reference)},
    {DW_AT_static_link,		dw_classes (cl_block, cl_constant)},
    {DW_AT_type,		dw_classes (cl_reference)},
    {DW_AT_use_location,	dw_classes (cl_block, cl_constant)},
    {DW_AT_variable_parameter,	dw_classes (cl_flag)},
    {DW_AT_virtuality,		dw_classes (cl_constant)},
    {DW_AT_vtable_elem_location, dw_classes (cl_block, cl_reference)},
    {0,				dw_classes ()}
  };

  dwarf_row const dwarf_2_form_table_rows[] = {
    {DW_FORM_addr,   dw_classes (cl_address)},
    {DW_FORM_block2, dw_classes (cl_block)},
    {DW_FORM_block4, dw_classes (cl_block)},
    {DW_FORM_data2,  dw_classes (cl_constant)},
    {DW_FORM_data4,  dw_classes (cl_constant)},
    {DW_FORM_data8,  dw_classes (cl_constant)},
    {DW_FORM_string, dw_classes (cl_string)},
    {DW_FORM_block,  dw_classes (cl_block)},
    {DW_FORM_block1, dw_classes (cl_block)},
    {DW_FORM_data1,  dw_classes (cl_constant)},
    {DW_FORM_flag,   dw_classes (cl_flag)},
    {DW_FORM_sdata,  dw_classes (cl_constant)},
    {DW_FORM_strp,   dw_classes (cl_string)},
    {DW_FORM_udata,  dw_classes (cl_constant)},
    {DW_FORM_ref_addr, dw_classes (cl_reference)},
    {DW_FORM_ref1,   dw_classes (cl_reference)},
    {DW_FORM_ref2,   dw_classes (cl_reference)},
    {DW_FORM_ref4,   dw_classes (cl_reference)},
    {DW_FORM_ref8,   dw_classes (cl_reference)},
    {DW_FORM_ref_udata, dw_classes (cl_reference)},
    {0,	             dw_classes ()}
  };
  form_table const dwarf_2_form_table (dwarf_2_form_table_rows);

  /* Changes from dwarf_2_*_table:  */
  dwarf_row const dwarf_3_at_table[] = {
    {DW_AT_location,	dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_byte_size,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_bit_offset,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_bit_size,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_stmt_list,	dw_classes (cl_lineptr)},
    {DW_AT_string_length, dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_const_value,	dw_classes (cl_block, cl_constant, cl_string)},
    {DW_AT_lower_bound,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_return_addr,	dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_bit_stride,	dw_classes (cl_constant)},
    {DW_AT_upper_bound,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_count,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_data_member_location, dw_classes (cl_block, cl_constant,
					     cl_loclistptr)},
    {DW_AT_frame_base,	dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_macro_info,	dw_classes (cl_macptr)},
    {DW_AT_segment,	dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_static_link,	dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_use_location, dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_vtable_elem_location, dw_classes (cl_block, cl_loclistptr)},
    {DW_AT_associated,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_data_location, dw_classes (cl_block)},
    {DW_AT_byte_stride,	dw_classes (cl_block, cl_constant, cl_reference)},
    {DW_AT_entry_pc,	dw_classes (cl_address)},
    {DW_AT_use_UTF8,	dw_classes (cl_flag)},
    {DW_AT_extension,	dw_classes (cl_reference)},
    {DW_AT_ranges,	dw_classes (cl_rangelistptr)},
    {DW_AT_trampoline,	dw_classes (cl_address, cl_flag, cl_reference,
				    cl_string)},
    {DW_AT_call_column,	dw_classes (cl_constant)},
    {DW_AT_call_file,	dw_classes (cl_constant)},
    {DW_AT_call_line,	dw_classes (cl_constant)},
    {DW_AT_description,	dw_classes (cl_string)},
    {DW_AT_binary_scale, dw_classes (cl_constant)},
    {DW_AT_decimal_scale, dw_classes (cl_constant)},
    {DW_AT_small,	dw_classes (cl_reference)},
    {DW_AT_decimal_sign, dw_classes (cl_constant)},
    {DW_AT_digit_count,	dw_classes (cl_constant)},
    {DW_AT_picture_string, dw_classes (cl_string)},
    {DW_AT_mutable,	dw_classes (cl_flag)},
    {DW_AT_threads_scaled, dw_classes (cl_flag)},
    {DW_AT_explicit,	dw_classes (cl_flag)},
    {DW_AT_object_pointer, dw_classes (cl_reference)},
    {DW_AT_endianity,	dw_classes (cl_constant)},
    {DW_AT_elemental,	dw_classes (cl_flag)},
    {DW_AT_pure,	dw_classes (cl_flag)},
    {DW_AT_recursive,	dw_classes (cl_flag)},
    {0,			dw_classes ()}
  };

  dwarf_row const dwarf_3_form_table_rows[] = {
    {DW_FORM_data4,	dw_classes (cl_constant, cl_lineptr, cl_loclistptr,
				    cl_macptr, cl_rangelistptr)},
    {DW_FORM_data8,	dw_classes (cl_constant, cl_lineptr, cl_loclistptr,
				    cl_macptr, cl_rangelistptr)},
    {0,			dw_classes ()}
  };
  form_table const dwarf_3_form_table (dwarf_3_form_table_rows, &dwarf_2_form_table);

  /* Changes from dwarf_3_*_table:  */
  dwarf_row const dwarf_4_at_table[] = {
    {DW_AT_location,	dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_bit_offset,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_bit_size,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_high_pc,	dw_classes (cl_address, cl_constant)},
    {DW_AT_string_length, dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_const_value,	dw_classes (cl_block, cl_constant, cl_string)},
    {DW_AT_lower_bound,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_return_addr,	dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_bit_stride,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_upper_bound,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_count,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_data_member_location, dw_classes (cl_constant, cl_exprloc,
					     cl_loclistptr)},
    {DW_AT_frame_base,	dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_namelist_item, dw_classes (cl_reference)},
    {DW_AT_segment,	dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_static_link,	dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_use_location, dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_vtable_elem_location, dw_classes (cl_exprloc, cl_loclistptr)},
    {DW_AT_allocated,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_associated,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_data_location, dw_classes (cl_exprloc)},
    {DW_AT_byte_stride,	dw_classes (cl_constant, cl_exprloc, cl_reference)},
    {DW_AT_signature,	dw_classes (cl_reference)},
    {DW_AT_main_subprogram, dw_classes (cl_flag)},
    {DW_AT_data_bit_offset, dw_classes (cl_constant)},
    {DW_AT_const_expr,	dw_classes (cl_flag)},
    {0,			dw_classes ()}
  };

  dwarf_row const dwarf_4_form_table_rows[] = {
    {DW_FORM_data4,		dw_classes (cl_constant)},
    {DW_FORM_data8,		dw_classes (cl_constant)},
    {DW_FORM_sec_offset,	dw_classes (cl_lineptr, cl_loclistptr,
					    cl_macptr, cl_rangelistptr)},
    {DW_FORM_exprloc,		dw_classes (cl_exprloc)},
    {DW_FORM_flag_present,	dw_classes (cl_flag)},
    {DW_FORM_ref_sig8,		dw_classes (cl_reference)},
    {0,				dw_classes ()}
  };
  form_table const dwarf_4_form_table (dwarf_4_form_table_rows, &dwarf_3_form_table);

  class std_dwarf
    : public dwarf_version
  {
    typedef std::map <attr, form_set_t> _forms_t;
    _forms_t m_forms;
    dwarf_version const *m_parent;
    form_table const &m_formtab;

  public:
    std_dwarf (dwarf_row const attrtab[], form_table const &formtab,
	       dwarf_version const *parent = NULL)
      : m_parent (parent)
      , m_formtab (formtab)
    {
      for (unsigned i = 0; attrtab[i].val != 0; ++i)
	for (unsigned c = 0; c < attrtab[i].classes.size (); ++c)
	  if (attrtab[i].classes[c])
	    m_forms[attrtab[i].val]
	      = formtab.get (static_cast <dw_class> (c));

      /*
	std::cout << pri::attr (attrtab[i].at) << " {";
	for (std::set <int>::const_iterator it = forms.begin ();
	it != forms.end (); ++it)
	std::cout << (it == forms.begin () ? "" : ", ") << pri::form (*it);
	std::cout << "}" << std::endl;
      */
    }

    form_set_t const &allowed_forms () const
    {
      return m_formtab.get_all ();
    }

    form_set_t const &allowed_forms (attr at) const
    {
      _forms_t::const_iterator it = m_forms.find (at);
      assert (it != m_forms.end () || m_parent != NULL);
      if (it != m_forms.end ())
	return it->second;
      else
	return m_parent->allowed_forms (at);
    }

    // Answer forms allowed at DIE with that tag.
    form_set_t const &allowed_forms (attr at,
				     __attribute__ ((unused)) die_tag tag) const
    {
      return allowed_forms (at);
    }
  };

  std_dwarf dwarf2 (dwarf_2_at_table, dwarf_2_form_table);
  std_dwarf dwarf3 (dwarf_3_at_table, dwarf_3_form_table, &dwarf2);
  std_dwarf dwarf4 (dwarf_4_at_table, dwarf_4_form_table, &dwarf3);
}

dwarf_version_h
get_dwarf_version (unsigned version)
{
  switch (version)
    {
    case 2: return &dwarf2;
    case 3: return &dwarf3;
    case 4: return &dwarf4;
    default: return NULL;
    };
}

dwarf_version_h
get_latest_dwarf_version ()
{
  return &dwarf4;
}

bool
dwver_form_valid (dwarf_version const *ver, int form)
{
  return ver->form_allowed (form);
}

bool
dwver_form_allowed (dwarf_version const *ver, int attr, int form)
{
  return ver->form_allowed (attr, form);
}

bool
dwver_form_allowed_in (dwarf_version const *ver, int attr, int form, int tag)
{
  return ver->form_allowed (attr, form, tag);
}

int
dwver_check_sibling_form (dwarf_version_h ver, int form)
{
  if (!dwver_form_allowed (ver, DW_AT_sibling, form))
    return -2;
  else if (form == DW_FORM_ref_addr)
    return -1;
  else
    return 0;
}

#if 0

.at (DW_AT_abstract_origin)
.version (dwarf_2, dwarf_3, dwarf_4).classes (reference)

.ad (DW_AT_accessibility)
.version (dwarf_2, dwarf_3, dwarf_4).classes (constant)

.at (DW_AT_allocated)
.version (dwarf_3).classes (constant, block, reference)
.version (dwarf_4).classes (constant, exprloc, reference)

;


{DW_AT_abstract_origin, 		0x31	2,3,4	reference

DW_AT_accessibility		0x32	2,3,4	constant

DW_AT_address_class		0x33	2,3,4	constant


// compositions of dwarf_version:
//  - extend (A, B): allow union of A and B
//  - strict (A, B): allow intersection of A and B

// AT->class
DW_AT_abstract_origin		0x31	/*2,3,4*/reference

DW_AT_accessibility		0x32	/*2,3,4*/constant

DW_AT_address_class		0x33	/*2,3,4*/constant

DW_AT_allocated			0x4e	/*3*/constant,block,reference
DW_AT_allocated			0x4e	/*4*/constant,exprloc,reference

DW_AT_artificial		0x34	/*2,3,4*/flag

DW_AT_associated		0x4f	/*3*/block,constant,reference
DW_AT_associated		0x4f	/*4*/constant,exprloc,reference

DW_AT_base_types		0x35	/*2,3,4*/reference

DW_AT_binary_scale		0x5b	/*3,4*/constant

DW_AT_bit_offset		0x0c	/*2*/constant
DW_AT_bit_offset		0x0c	/*3*/block,constant,reference
DW_AT_bit_offset		0x0c	/*4*/constant,exprloc,reference

DW_AT_bit_size			0x0d	/*2*/constant
DW_AT_bit_size			0x0d	/*3*/block,constant,reference
DW_AT_bit_size			0x0d	/*4*/constant,exprloc,reference

DW_AT_bit_stride		0x2e	/*3*/constant
DW_AT_bit_stride		0x2e	/*4*/constant,exprloc,reference

DW_AT_byte_size			0x0b	/*2*/constant
DW_AT_byte_size			0x0b	/*3*/block,constant,reference
DW_AT_byte_size			0x0b	/*4*/constant,exprloc,reference

DW_AT_byte_stride		0x51	/*3*/block,constant,reference
DW_AT_byte_stride		0x51	/*4*/constant,exprloc,reference

DW_AT_call_column		0x57	/*3,4*/constant

DW_AT_call_file			0x58	/*3,4*/constant

DW_AT_call_line			0x59	/*3,4*/constant

DW_AT_calling_convention	0x36	/*2,3,4*/constant

DW_AT_common_reference		0x1a	/*2,3,4*/reference

DW_AT_comp_dir			0x1b	/*2,3,4*/string

DW_AT_const_expr		0x6c	/*4*/flag

DW_AT_const_value		0x1c	/*2*/string,constant,block
DW_AT_const_value		0x1c	/*3*/block,constant,string
DW_AT_const_value		0x1c	/*4*/block,constant,string

DW_AT_containing_type		0x1d	/*2,3,4*/reference

DW_AT_count			0x37	/*2*/constant,reference
DW_AT_count			0x37	/*3*/block,constant,reference
DW_AT_count			0x37	/*4*/constant,exprloc,reference

DW_AT_data_bit_offset		0x6b	/*4*/constant

DW_AT_data_location		0x50	/*3*/block
DW_AT_data_location		0x50	/*4*/exprloc

DW_AT_data_member_location	0x38	/*2*/block,reference
DW_AT_data_member_location	0x38	/*3*/block,constant,loclistptr
DW_AT_data_member_location	0x38	/*4*/constant,exprloc,loclistptr

DW_AT_decimal_scale		0x5c	/*3,4*/constant

DW_AT_decimal_sign		0x5e	/*3,4*/constant

DW_AT_decl_column		0x39	/*2,3,4*/constant

DW_AT_decl_file			0x3a	/*2,3,4*/constant

DW_AT_decl_line			0x3b	/*2,3,4*/constant

DW_AT_declaration		0x3c	/*2,3,4*/flag

DW_AT_default_value		0x1e	/*2,3,4*/reference

DW_AT_description		0x5a	/*3,4*/string

DW_AT_digit_count		0x5f	/*3,4*/constant

DW_AT_discr			0x15	/*2,3,4*/reference

DW_AT_discr_list		0x3d	/*2,3,4*/block

DW_AT_discr_value		0x16	/*2,3,4*/constant

DW_AT_elemental			0x66	/*3,4*/flag

DW_AT_encoding			0x3e	/*2,3,4*/constant

DW_AT_endianity			0x65	/*3,4*/constant

DW_AT_entry_pc			0x52	/*3,4*/address

DW_AT_explicit			0x63	/*3,4*/flag

DW_AT_extension			0x54	/*3,4*/reference

DW_AT_external			0x3f	/*2,3,4*/flag

DW_AT_frame_base		0x40	/*2*/block,constant
DW_AT_frame_base		0x40	/*3*/block,loclistptr
DW_AT_frame_base		0x40	/*4*/exprloc,loclistptr

DW_AT_friend			0x41	/*2,3,4*/reference

DW_AT_high_pc			0x12	/*2,3*/address
DW_AT_high_pc			0x12	/*4*/address,constant

DW_AT_identifier_case		0x42	/*2,3,4*/constant

DW_AT_import			0x18	/*2,3,4*/reference

DW_AT_inline			0x20	/*2,3,4*/constant

DW_AT_is_optional		0x21	/*2,3,4*/flag

DW_AT_language			0x13	/*2,3,4*/constant

DW_AT_location			0x02	/*2*/block,constant
DW_AT_location			0x02	/*3*/block,loclistptr
DW_AT_location			0x02	/*4*/exprloc,loclistptr

DW_AT_low_pc			0x11	/*2,3,4*/address

DW_AT_lower_bound		0x22	/*2*/constant,reference
DW_AT_lower_bound		0x22	/*3*/block,constant,reference
DW_AT_lower_bound		0x22	/*4*/constant,exprloc,reference

DW_AT_macro_info		0x43	/*2*/constant
DW_AT_macro_info		0x43	/*3,4*/macptr

DW_AT_main_subprogram		0x6a	/*4*/flag

DW_AT_mutable			0x61	/*3,4*/flag

DW_AT_name			0x03	/*2,3,4*/string

DW_AT_namelist_item		0x44	/*2,3*/block
DW_AT_namelist_item		0x44	/*4*/reference

DW_AT_object_pointer		0x64	/*3,4*/reference

DW_AT_ordering			0x09	/*2,3,4*/constant

DW_AT_picture_string		0x60	/*3,4*/string

DW_AT_priority			0x45	/*2,3,4*/reference

DW_AT_producer			0x25	/*2,3,4*/string

DW_AT_prototyped		0x27	/*2,3,4*/flag

DW_AT_pure			0x67	/*3,4*/flag

DW_AT_ranges			0x55	/*3,4*/rangelistptr

DW_AT_recursive			0x68	/*3,4*/flag

DW_AT_return_addr		0x2a	/*2*/block,constant
DW_AT_return_addr		0x2a	/*3*/block,loclistptr
DW_AT_return_addr		0x2a	/*4*/exprloc,loclistptr

DW_AT_segment			0x46	/*2*/block,constant
DW_AT_segment			0x46	/*3*/block,loclistptr
DW_AT_segment			0x46	/*4*/exprloc,loclistptr

DW_AT_sibling			0x01	/*2,3,4*/reference

DW_AT_signature			0x69	/*4*/reference

DW_AT_small			0x5d	/*3,4*/reference

DW_AT_specification		0x47	/*2,3,4*/reference

DW_AT_start_scope		0x2c	/*2,3,4*/constant

DW_AT_static_link		0x48	/*2*/block,constant
DW_AT_static_link		0x48	/*3*/block,loclistptr
DW_AT_static_link		0x48	/*4*/exprloc,loclistptr

DW_AT_stmt_list			0x10	/*2*/constant
DW_AT_stmt_list			0x10	/*3,4*/lineptr

DW_AT_stride_size		0x2e	/*2*/constant

DW_AT_string_length		0x19	/*2*/block,constant
DW_AT_string_length		0x19	/*3*/block,loclistptr
DW_AT_string_length		0x19	/*4*/exprloc,loclistptr

DW_AT_threads_scaled		0x62	/*3,4*/flag

DW_AT_trampoline		0x56	/*3,4*/address,flag,reference,string

DW_AT_type			0x49	/*2,3,4*/reference

DW_AT_upper_bound		0x2f	/*2*/constant
DW_AT_upper_bound		0x2f	/*3*/block,constant,reference
DW_AT_upper_bound		0x2f	/*4*/constant,exprloc,reference

DW_AT_use_UTF8			0x53	/*3,4*/flag

DW_AT_use_location		0x4a	/*2*/block,constant
DW_AT_use_location		0x4a	/*3*/block,loclistptr
DW_AT_use_location		0x4a	/*4*/exprloc,loclistptr

DW_AT_variable_parameter	0x4b	/*2,3,4*/flag

DW_AT_virtuality		0x4c	/*2,3,4*/constant

DW_AT_visibility		0x17	/*2,3,4*/constant

DW_AT_vtable_elem_location	0x4d	/*2*/block,reference
DW_AT_vtable_elem_location	0x4d	/*3*/block,loclistptr
DW_AT_vtable_elem_location	0x4d	/*4*/exprloc,loclistptr


// FORM->class
DW_FORM_addr		0x01	/*2,3,4*/address

DW_FORM_block		0x09	/*2,3,4*/block

DW_FORM_block1		0x0a	/*2,3,4*/block

DW_FORM_block2		0x03	/*2,3,4*/block

DW_FORM_block4		0x04	/*2,3,4*/block

DW_FORM_data1		0x0b	/*2,3,4*/constant

DW_FORM_data2		0x05	/*2,3,4*/constant

DW_FORM_data4		0x06	/*2,4*/constant
DW_FORM_data4		0x06	/*3*/constant, lineptr, loclistptr, macptr, rangelistptr

DW_FORM_data8		0x07	/*2,4*/constant
DW_FORM_data8		0x07	/*3*/constant, lineptr, loclistptr, macptr, rangelistptr

DW_FORM_exprloc		0x18	/*4*/exprloc

DW_FORM_flag		0x0c	/*2,3,4*/flag

DW_FORM_flag_present	0x19	/*4*/flag

DW_FORM_indirect	0x16	/*2,3,4*/-

DW_FORM_ref1		0x11	/*2,3,4*/reference

DW_FORM_ref2		0x12	/*2,3,4*/reference

DW_FORM_ref4		0x13	/*2,3,4*/reference

DW_FORM_ref8		0x14	/*2,3,4*/reference

DW_FORM_ref_addr	0x10	/*2,3,4*/reference

DW_FORM_ref_sig8	0x20	/*4*/reference

DW_FORM_ref_udata	0x15	/*2,3,4*/reference

DW_FORM_sdata		0x0d	/*2,3,4*/constant

DW_FORM_sec_offset	0x17	/*4*/lineptr, loclistptr, macptr, rangelistptr

DW_FORM_string		0x08	/*2,3,4*/string

DW_FORM_strp		0x0e	/*2,3,4*/string

DW_FORM_udata		0x0f	/*2,3,4*/constant

#endif
