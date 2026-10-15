#include "homeassistant.h"

namespace esphome::homeassistant {

PROGMEM_STRING_TABLE(UnsetValues, "None", "unavailable", "unknown");

bool is_unset(StringRef value) { return find_in_table<UnsetValues>(value) >= 0; }

}  // namespace esphome::homeassistant
