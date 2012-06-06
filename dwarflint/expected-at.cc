/* Pedantic checking of DWARF files.
   Copyright (C) 2009, 2011 Red Hat, Inc.
   This file is part of elfutils.
   Written by Petr Machata <pmachata@redhat.com>, 2009.

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

#include <config.h>
#include "expected.hh"
#include "../libdw/dwarf.h"

expected_at_map::expected_at_map ()
{
  std::set <int> at_set_decl;
  at_set_decl.insert (DW_AT_decl_column);
  at_set_decl.insert (DW_AT_decl_file);
  at_set_decl.insert (DW_AT_decl_line);

  std::set <int> at_linkage_name;
  at_linkage_name.insert (DW_AT_MIPS_linkage_name);
  at_linkage_name.insert (DW_AT_linkage_name);

  m_map [DW_TAG_access_declaration]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    ;

  m_map[DW_TAG_array_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_bit_stride)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_ordering)
    .optional (DW_AT_sibling)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    .optional (DW_AT_GNU_vector)
    ;

  m_map [DW_TAG_base_type]
    .optional (at_set_decl)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_binary_scale)
    .optional (DW_AT_bit_offset)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_bit_offset)
    .optional (DW_AT_data_location)
    .optional (DW_AT_decimal_scale)
    .optional (DW_AT_decimal_sign)
    .optional (DW_AT_description)
    .optional (DW_AT_digit_count)
    .optional (DW_AT_encoding)
    .optional (DW_AT_endianity)
    .optional (DW_AT_name)
    .optional (DW_AT_picture_string)
    .optional (DW_AT_sibling)
    .optional (DW_AT_small)
    ;

  m_map [DW_TAG_catch_block]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_class_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_signature)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_visibility)
    .optional (DW_AT_containing_type) // XXX added to reflect reality
    ;

  m_map [DW_TAG_common_block]
    .optional (at_set_decl)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (at_linkage_name)
    .optional (DW_AT_location)
    .optional (DW_AT_name)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_common_inclusion]
    .optional (at_set_decl)
    .optional (DW_AT_common_reference)
    .optional (DW_AT_declaration)
    .optional (DW_AT_sibling)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_compile_unit]
    .optional (DW_AT_base_types)
    .optional (DW_AT_comp_dir)
    .optional (DW_AT_identifier_case)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_language)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_macro_info)
    .optional (DW_AT_main_subprogram)
    .optional (DW_AT_name)
    .optional (DW_AT_producer)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_stmt_list)
    .optional (DW_AT_use_UTF8)
    .optional (DW_AT_entry_pc) // XXX added to reflect reality
    ;

  m_map [DW_TAG_condition]
    .optional (at_set_decl)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_const_type]
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_constant]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_const_value)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_endianity)
    .optional (DW_AT_external)
    .optional (at_linkage_name)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_dwarf_procedure]
    .optional (DW_AT_location)
    ;

  m_map [DW_TAG_entry_point]
    .optional (at_set_decl)
    .optional (DW_AT_address_class)
    .optional (DW_AT_description)
    .optional (DW_AT_frame_base)
    .optional (at_linkage_name)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_name)
    .optional (DW_AT_return_addr)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_static_link)
    .optional (DW_AT_type)
    .optional (DW_AT_GNU_all_tail_call_sites)
    .optional (DW_AT_GNU_all_call_sites)
    .optional (DW_AT_GNU_all_source_call_sites)
    ;

  m_map [DW_TAG_enumeration_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_bit_stride)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_byte_stride)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_signature)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    .optional (at_linkage_name) // GNU extension for anonymous typedef enums.
    ;

  m_map [DW_TAG_enumerator]
    .optional (at_set_decl)
    .optional (DW_AT_const_value)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_file_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_formal_parameter]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_artificial)
    .optional (DW_AT_const_value)
    .optional (DW_AT_default_value)
    .optional (DW_AT_description)
    .optional (DW_AT_endianity)
    .optional (DW_AT_is_optional)
    .optional (DW_AT_location)
    .optional (DW_AT_name)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_variable_parameter)
    ;

  m_map [DW_TAG_friend]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_friend)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_imported_declaration]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_description)
    .optional (DW_AT_import)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    ;

  m_map [DW_TAG_imported_module]
    .optional (at_set_decl)
    .optional (DW_AT_import)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    ;

  m_map [DW_TAG_imported_unit]
    .required (DW_AT_import)
    ;

  m_map [DW_TAG_inheritance]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_data_member_location)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_virtuality)
    ;

  m_map [DW_TAG_inlined_subroutine]
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_call_column)
    .optional (DW_AT_call_file)
    .optional (DW_AT_call_line)
    .optional (DW_AT_const_expr)
    .optional (DW_AT_entry_pc)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_ranges)
    .optional (DW_AT_return_addr)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_trampoline)
    ;

  m_map [DW_TAG_interface_type]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    ;

  m_map [DW_TAG_label]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_description)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_name)
    .optional (DW_AT_segment)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_lexical_block]
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_description)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_name)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    ;

  // At one time gcc did emit at_linkage_name for members, but that
  // has been corrected:
  // http://gcc.gnu.org/ml/gcc-patches/2010-06/msg01713.html
  m_map [DW_TAG_member]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_bit_offset)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_bit_offset)
    .optional (DW_AT_data_member_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_mutable)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_module]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_entry_pc)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_name)
    .optional (DW_AT_priority)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_specification)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_namelist]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_declaration)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_namelist_item]
    .optional (at_set_decl)
    .optional (DW_AT_namelist_item)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_namespace]
    .optional (at_set_decl)
    .optional (DW_AT_description)
    .optional (DW_AT_extension)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    ;

  m_map [DW_TAG_packed_type]
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_partial_unit]
    .optional (DW_AT_base_types)
    .optional (DW_AT_comp_dir)
    .optional (DW_AT_description)
    .optional (DW_AT_identifier_case)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_language)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_macro_info)
    .optional (DW_AT_name)
    .optional (DW_AT_producer)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_stmt_list)
    .optional (DW_AT_use_UTF8)
    ;

  m_map [DW_TAG_pointer_type]
    .optional (DW_AT_address_class)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_byte_size) // XXX added to reflect reality
    ;

  m_map [DW_TAG_ptr_to_member_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_address_class)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_containing_type)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_use_location)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_reference_type]
    .optional (DW_AT_address_class)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_byte_size) // XXX added to reflect reality
    ;

  m_map [DW_TAG_restrict_type]
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_rvalue_reference_type]
    .optional (at_set_decl)
    .optional (DW_AT_address_class)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_set_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_shared_type]
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_count)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_string_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_string_length)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_structure_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_signature)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_visibility)
    .optional (DW_AT_containing_type) // XXX added to reflect reality
    .optional (at_linkage_name) // GNU extension for anonymous typedef structs.
    ;

  m_map [DW_TAG_subprogram]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_address_class)
    .optional (DW_AT_artificial)
    .optional (DW_AT_calling_convention)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_elemental)
    .optional (DW_AT_entry_pc)
    .optional (DW_AT_explicit)
    .optional (DW_AT_external)
    .optional (DW_AT_frame_base)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_inline)
    .optional (at_linkage_name)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_name)
    .optional (DW_AT_object_pointer)
    .optional (DW_AT_prototyped)
    .optional (DW_AT_pure)
    .optional (DW_AT_ranges)
    .optional (DW_AT_recursive)
    .optional (DW_AT_return_addr)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_static_link)
    .optional (DW_AT_trampoline)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    .optional (DW_AT_virtuality)
    .optional (DW_AT_vtable_elem_location)
    .optional (DW_AT_containing_type) // XXX added to reflect reality
    .optional (DW_AT_GNU_all_tail_call_sites)
    .optional (DW_AT_GNU_all_call_sites)
    .optional (DW_AT_GNU_all_source_call_sites)
    ;

  m_map [DW_TAG_subrange_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_bit_stride)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_byte_stride)
    .optional (DW_AT_count)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_lower_bound)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_threads_scaled)
    .optional (DW_AT_type)
    .optional (DW_AT_upper_bound)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_subroutine_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_address_class)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_prototyped)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_template_alias]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_signature)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_template_type_parameter]
    .optional (at_set_decl)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_template_value_parameter ]
    .optional (at_set_decl)
    .optional (DW_AT_const_value)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_thrown_type]
    .optional (at_set_decl)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_try_block]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_typedef]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  m_map [DW_TAG_type_unit]
    .optional (DW_AT_language)
    ;

  m_map [DW_TAG_union_type]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_bit_size)
    .optional (DW_AT_byte_size)
    .optional (DW_AT_data_location)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_signature)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_visibility)
    .optional (at_linkage_name) // GNU extension for anonymous typedef unions.
    ;

  m_map [DW_TAG_unspecified_parameters]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_artificial)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_unspecified_type]
    .optional (at_set_decl)
    .optional (DW_AT_description)
    .optional (DW_AT_name)
    ;

  m_map [DW_TAG_variable]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_const_expr)
    .optional (DW_AT_const_value)
    .optional (DW_AT_declaration)
    .optional (DW_AT_description)
    .optional (DW_AT_endianity)
    .optional (DW_AT_external)
    .optional (at_linkage_name)
    .optional (DW_AT_location)
    .optional (DW_AT_name)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_specification)
    .optional (DW_AT_start_scope)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    .optional (DW_AT_artificial) // XXX added to reflect reality
    ;

  m_map [DW_TAG_variant]
    .optional (at_set_decl)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_declaration)
    .optional (DW_AT_discr_list)
    .optional (DW_AT_discr_value)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_variant_part]
    .optional (at_set_decl)
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_accessibility)
    .optional (DW_AT_declaration)
    .optional (DW_AT_discr)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_volatile_type]
    .optional (DW_AT_allocated)
    .optional (DW_AT_associated)
    .optional (DW_AT_data_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_with_stmt]
    .optional (DW_AT_accessibility)
    .optional (DW_AT_address_class)
    .optional (DW_AT_declaration)
    .optional (DW_AT_high_pc)
    .optional (DW_AT_location)
    .optional (DW_AT_low_pc)
    .optional (DW_AT_ranges)
    .optional (DW_AT_segment)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    .optional (DW_AT_visibility)
    ;

  // http://gcc.gnu.org/wiki/summit2010?action=AttachFile&do=get&target=jelinek.pdf
  m_map [DW_TAG_GNU_call_site]
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_call_column)
    .optional (DW_AT_call_file)
    .optional (DW_AT_call_line)
    .optional (DW_AT_GNU_call_site_target)
    .optional (DW_AT_GNU_call_site_target_clobbered)
    .required (DW_AT_low_pc)
    .optional (DW_AT_sibling)
    .optional (DW_AT_GNU_tail_call)
    .optional (DW_AT_type)
    ;

  m_map [DW_TAG_GNU_call_site_parameter]
    .optional (DW_AT_abstract_origin)
    .optional (DW_AT_GNU_call_site_data_value)
    .optional (DW_AT_GNU_call_site_value)
    .optional (DW_AT_data_location)
    .required (DW_AT_location)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    .optional (DW_AT_type)
    ;

  // http://gcc.gnu.org/wiki/TemplateParmsDwarf
  m_map [DW_TAG_GNU_template_template_param]
    .required (DW_AT_name)
    .required (DW_AT_GNU_template_name)
    ;

  m_map [DW_TAG_GNU_template_parameter_pack]
    .optional (at_set_decl)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    ;

  m_map [DW_TAG_GNU_formal_parameter_pack]
    .optional (at_set_decl)
    .optional (DW_AT_name)
    .optional (DW_AT_sibling)
    ;

}
