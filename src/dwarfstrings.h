#ifndef DWARFSTRINGS_H
#define DWARFSTRINGS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

const char *dwarf_tag_string (unsigned int tag);

const char *dwarf_attr_string (unsigned int attrnum);

const char *dwarf_form_string (unsigned int form);

const char *dwarf_lang_string (unsigned int lang);

const char *dwarf_inline_string (unsigned int code);

const char *dwarf_encoding_string (unsigned int code);

const char *dwarf_access_string (unsigned int code);

const char *dwarf_visibility_string (unsigned int code);

const char *dwarf_virtuality_string (unsigned int code);

const char *dwarf_identifier_case_string (unsigned int code);

const char *dwarf_calling_convention_string (unsigned int code);

const char *dwarf_ordering_string (unsigned int code);

const char *dwarf_discr_list_string (unsigned int code);

const char *dwarf_locexpr_opcode_string (unsigned int code);

#ifdef __cplusplus
}
#endif

#endif  /* dwarfstrings.h */
