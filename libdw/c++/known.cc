#include <config.h>
#include "dwarf"
#include "known-dwarf.h"

using namespace elfutils;
using namespace std;


const char *
dwarf::known_tag (int tag)
{
  switch (tag)
    {
#define ONE_KNOWN_DW_TAG(name, id)		case id: return #name;
#define ONE_KNOWN_DW_TAG_DESC(name, id, desc)	ONE_KNOWN_DW_TAG (name, id)
      ALL_KNOWN_DW_TAG
    }
  return NULL;
}

const char *
dwarf::known_attribute (int name)
{
  switch (name)
    {
#define ONE_KNOWN_DW_AT(name, id)		case id: return #name;
#define ONE_KNOWN_DW_AT_DESC(name, id, desc)	ONE_KNOWN_DW_AT (name, id)
      ALL_KNOWN_DW_AT
    }
  return NULL;
}
