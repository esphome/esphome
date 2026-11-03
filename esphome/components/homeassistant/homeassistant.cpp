#include "homeassistant.h"

#include "esphome/core/progmem.h"

namespace esphome::homeassistant {

PROGMEM_STRING_TABLE(UnsetValues, "None", "unavailable", "unknown");

bool is_unset(StringRef value) { return progmem_string_table_find<UnsetValues>(value.c_str(), value.size()) >= 0; }

}  // namespace esphome::homeassistant
