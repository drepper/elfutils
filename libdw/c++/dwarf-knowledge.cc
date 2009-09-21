#include <config.h>
#include "dwarf"

using namespace std;
using namespace elfutils;

#define VS(what)	(1U << dwarf::VS_##what)

/* Return a bitmask of value spaces expected for this attribute of this tag.
   Primarily culled from the DWARF 3 spec: 7.5.4, Figure 20.  */

static unsigned int
expected_value_space (int attr, int tag)
{

  switch (attr)
    {
    case DW_AT_sibling:
    case DW_AT_common_reference:
    case DW_AT_containing_type:
    case DW_AT_default_value:
    case DW_AT_abstract_origin:
    case DW_AT_base_types:
    case DW_AT_friend:
    case DW_AT_priority:
    case DW_AT_specification:
    case DW_AT_type:
    case DW_AT_use_location:
    case DW_AT_data_location:
    case DW_AT_extension:
    case DW_AT_small:
    case DW_AT_object_pointer:
    case DW_AT_namelist_item:
      return VS(reference);

    case DW_AT_location:
    case DW_AT_string_length:
    case DW_AT_return_addr:
    case DW_AT_frame_base:
    case DW_AT_segment:
    case DW_AT_static_link:
    case DW_AT_vtable_elem_location:
      return VS(location);

    case DW_AT_data_member_location:
      return VS(location) | VS(constant);

    case DW_AT_name:
      switch (tag)
	{
	case DW_TAG_compile_unit:
	case DW_TAG_partial_unit:
	  return VS(source_file);
	default:
	  return VS(identifier);
	}

    case DW_AT_ordering:
    case DW_AT_language:
    case DW_AT_visibility:
    case DW_AT_inline:
    case DW_AT_accessibility:
    case DW_AT_address_class:
    case DW_AT_calling_convention:
    case DW_AT_encoding:
    case DW_AT_identifier_case:
    case DW_AT_virtuality:
    case DW_AT_endianity:
      return VS(dwarf_constant);

    case DW_AT_byte_size:
    case DW_AT_byte_stride:
    case DW_AT_bit_size:
    case DW_AT_bit_offset:
    case DW_AT_bit_stride:
    case DW_AT_lower_bound:
    case DW_AT_upper_bound:
    case DW_AT_count:
    case DW_AT_allocated:
    case DW_AT_associated:
      return VS(reference) | VS(constant) | VS(location); // XXX non-loc expr

    case DW_AT_stmt_list:
      return VS(lineptr);
    case DW_AT_macro_info:
      return VS(macptr);
    case DW_AT_ranges:
      return VS(rangelistptr);

    case DW_AT_low_pc:
    case DW_AT_high_pc:
    case DW_AT_entry_pc:
      return VS(address);

    case DW_AT_discr:
      return VS(reference);
    case DW_AT_discr_value:
      return VS(constant);
    case DW_AT_discr_list:
      return VS(discr_list);

    case DW_AT_import:
      return VS(reference);

    case DW_AT_comp_dir:
      return VS(source_file);

    case DW_AT_const_value:
      return VS(constant) | VS(string) | VS(address);

    case DW_AT_is_optional:
    case DW_AT_prototyped:
    case DW_AT_artificial:
    case DW_AT_declaration:
    case DW_AT_external:
    case DW_AT_variable_parameter:
    case DW_AT_use_UTF8:
    case DW_AT_mutable:
    case DW_AT_threads_scaled:
    case DW_AT_explicit:
    case DW_AT_elemental:
    case DW_AT_pure:
    case DW_AT_recursive:
      return VS(flag);

    case DW_AT_producer:
      return VS(string);

    case DW_AT_start_scope:
      return VS(constant);

    case DW_AT_binary_scale:
    case DW_AT_decimal_scale:
    case DW_AT_decimal_sign:
    case DW_AT_digit_count:
      return VS(constant);

    case DW_AT_decl_file:
    case DW_AT_call_file:
      return VS(source_file);
    case DW_AT_decl_line:
    case DW_AT_call_line:
      return VS(source_line);
    case DW_AT_decl_column:
    case DW_AT_call_column:
      return VS(source_column);

    case DW_AT_trampoline:
      return VS(address) | VS(flag) | VS(reference) | VS(string);

    case DW_AT_description:
    case DW_AT_picture_string:
      return VS(string);

    case DW_AT_MIPS_linkage_name:
      return VS(identifier);
    }

  return 0;
}
