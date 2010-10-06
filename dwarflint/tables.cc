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
#include "check_debug_info.hh"

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

  enum storage_class_t {
    sc_value,
    sc_block,
    sc_string,
    max_storage_class
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
}

class form
{
public:
  virtual int name () const = 0;
  virtual dw_class_set const &classes () const = 0;
  virtual dwarf_version::form_width_t width (cu const *cu = NULL) const = 0;
  //virtual storage_class_t storage_class () const = 0;

  virtual ~form () {}
};

namespace {

  struct dwarf_row {
    int val;
    dw_class_set classes;
  };

  class x_form_table
  {
    typedef std::map<int, form const *> _forms_map_t;
    _forms_map_t const _m_forms;

    static _forms_map_t build_forms (form const *const *forms)
    {
      _forms_map_t ret;
      for (form const *const *it = forms; *it != NULL; ++it)
	ret[(*it)->name ()] = *it;
      return ret;
    }

  public:
    x_form_table (form const *const forms[])
      : _m_forms (build_forms (forms))
    {}

    form const *
    get (int f) const
    {
      _forms_map_t::const_iterator it = _m_forms.find (f);
      if (it != _m_forms.end ())
	return it->second;
      else
	return NULL;
    }

    bool
    has_form (int f) const
    {
      return get (f) != NULL;
    }
  };

  class basic_form
    : public form
  {
  protected:
    int _m_name;
    dw_class_set _m_classes;
  public:
    basic_form (int a_name, dw_class_set a_classes)
      : _m_name (a_name)
      , _m_classes (a_classes)
    {}

    int
    name () const
    {
      return _m_name;
    }

    dw_class_set const &
    classes () const
    {
      return _m_classes;
    }
  };

  class fixed_form
    : public basic_form
  {
  protected:
    dwarf_version::form_width_t _m_width;

  public:
    fixed_form (int a_name, dw_class_set a_classes,
		dwarf_version::form_width_t a_width)
      : basic_form (a_name, a_classes)
      , _m_width (a_width)
    {}

    dwarf_version::form_width_t
    width (__attribute__ ((unused)) struct cu const *cu = NULL) const
    {
      return _m_width;
    }
  };

  struct width_off {
    static dwarf_version::form_width_t width (struct cu const *cu) {
      return static_cast<dwarf_version::form_width_t> (cu->head->offset_size);
    }
  };

  struct width_addr {
    static dwarf_version::form_width_t width (struct cu const *cu) {
      return static_cast<dwarf_version::form_width_t> (cu->head->address_size);
    }
  };

  template<class Sel>
  class selwidth_form
    : public basic_form
  {
  public:
    template <class... Clss>
    selwidth_form (int a_name, Clss... a_classes)
      : basic_form (a_name, dw_classes (a_classes...))
    {}

    dwarf_version::form_width_t
    width (struct cu const *cu) const
    {
      return Sel::width (cu);
    }
  };

  typedef selwidth_form<width_off> w_off_form;
  typedef selwidth_form<width_addr> w_addr_form;

  template<dw_class... Clss>
  class simple_form
    : public fixed_form
  {
  public:
    simple_form (int a_name, dwarf_version::form_width_t a_width)
      : fixed_form (a_name, dw_classes (Clss...), a_width)
    {}
  };

  typedef simple_form<cl_block> block_form;
  typedef simple_form<cl_constant> const_form;
  typedef simple_form<cl_reference> ref_form;
  typedef simple_form<cl_flag> flag_form;

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

  form const *dwarf_2_forms[] = {
    new block_form (DW_FORM_block, dwarf_version::fw_leb),
    new block_form (DW_FORM_block1, dwarf_version::fw_1),
    new block_form (DW_FORM_block2, dwarf_version::fw_2),
    new block_form (DW_FORM_block4, dwarf_version::fw_4),

    new const_form (DW_FORM_data1, dwarf_version::fw_1),
    new const_form (DW_FORM_data2, dwarf_version::fw_2),
    new const_form (DW_FORM_data4, dwarf_version::fw_4),
    new const_form (DW_FORM_data8, dwarf_version::fw_8),
    new const_form (DW_FORM_sdata, dwarf_version::fw_leb),
    new const_form (DW_FORM_udata, dwarf_version::fw_leb),

    new flag_form (DW_FORM_flag, dwarf_version::fw_1),

    new ref_form (DW_FORM_ref1, dwarf_version::fw_1),
    new ref_form (DW_FORM_ref2, dwarf_version::fw_2),
    new ref_form (DW_FORM_ref4, dwarf_version::fw_4),
    new ref_form (DW_FORM_ref8, dwarf_version::fw_8),
    new ref_form (DW_FORM_ref_udata, dwarf_version::fw_leb),

    new fixed_form (DW_FORM_string, dw_classes (cl_string),
		    dwarf_version::fw_unknown),
    new w_off_form (DW_FORM_strp, cl_string),
    new w_addr_form (DW_FORM_addr, cl_address),
    new w_addr_form (DW_FORM_ref_addr, cl_reference),

    NULL
  };

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

  typedef simple_form<cl_constant, cl_lineptr, cl_loclistptr,
		      cl_macptr, cl_rangelistptr> dw3_data_form;
  form const *dwarf_3_forms[] = {
    new dw3_data_form (DW_FORM_data4, dwarf_version::fw_4),
    new dw3_data_form (DW_FORM_data8, dwarf_version::fw_8),
    new w_off_form (DW_FORM_ref_addr, cl_reference),
    NULL
  };

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

  form const *dwarf_4_forms[] = {
    new const_form (DW_FORM_data4, dwarf_version::fw_4),
    new const_form (DW_FORM_data8, dwarf_version::fw_8),
    new w_off_form (DW_FORM_sec_offset,
		    cl_lineptr, cl_loclistptr, cl_macptr, cl_rangelistptr),
    new simple_form<cl_exprloc> (DW_FORM_exprloc, dwarf_version::fw_leb),
    new flag_form (DW_FORM_flag_present, dwarf_version::fw_0),
    new ref_form (DW_FORM_ref_sig8, dwarf_version::fw_8),
    NULL
  };

  class std_dwarf
    : public dwarf_version
  {
    // attr_name->allowed_classes
    typedef std::map <int, dw_class_set> _attr_classes;
    _attr_classes const _m_attr_classes;
    x_form_table const _m_formtab;
    dwarf_version const *_m_parent;

    _attr_classes build_attr_classes (dwarf_row const attrtab[])
    {
      _attr_classes ret;
      for (unsigned i = 0; attrtab[i].val != 0; ++i)
	ret[attrtab[i].val] = attrtab[i].classes;
      return ret;
    }

  public:
    std_dwarf (dwarf_row const attrtab[],
	       form const *const forms[],
	       dwarf_version const *parent = NULL)
      : _m_attr_classes (build_attr_classes (attrtab))
      , _m_formtab (x_form_table (forms))
      , _m_parent (parent)
    {}

    form_width_t
    form_width (int name, struct cu const *cu = NULL) const
    {
      return get_form (name)->width (cu);
    }

    form const *
    get_form (int name) const
    {
      if (form const *form = _m_formtab.get (name))
	return form;
      else if (_m_parent != NULL)
	return _m_parent->get_form (name);
      else
	return NULL;
    }

    bool
    form_allowed (int form) const
    {
      return _m_formtab.has_form (form)
	|| (_m_parent != NULL && _m_parent->form_allowed (form));
    }

    dw_class_set const &
    get_attr_classes (int at) const
    {
      _attr_classes::const_iterator it = _m_attr_classes.find (at);
      if (it != _m_attr_classes.end ())
	return it->second;
      else
	{
	  assert (_m_parent != NULL);
	  if (std_dwarf const *std_parent
	      = dynamic_cast<std_dwarf const *> (_m_parent)) // xxx
	    return std_parent->get_attr_classes (at);
	}
      assert (!"Unsupported attribute!"); // xxx but I won't tell you which one
    }

    bool
    form_allowed (int attr_name, int form_name) const
    {
      dw_class_set const &attr_classes = get_attr_classes (attr_name);
      form const *form = this->get_form (form_name);
      assert (form != NULL);
      dw_class_set const &form_classes = form->classes ();
      return (attr_classes & form_classes).any ();
    }
  };

  std_dwarf dwarf2 (dwarf_2_at_table, dwarf_2_forms);
  std_dwarf dwarf3 (dwarf_3_at_table, dwarf_3_forms, &dwarf2);
  std_dwarf dwarf4 (dwarf_4_at_table, dwarf_4_forms, &dwarf3);
}

dwarf_version const *
dwarf_version::get (unsigned version)
{
  switch (version)
    {
    case 2: return &dwarf2;
    case 3: return &dwarf3;
    case 4: return &dwarf4;
    default: return NULL;
    };
}

dwarf_version const *
dwarf_version::get_latest ()
{
  return get (4);
}

int
dwarf_version::check_sibling_form (int form) const
{
  if (!form_allowed (DW_AT_sibling, form))
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
