#include "homeassistant.h"

#include "esphome/core/progmem.h"

namespace esphome::homeassistant {

PROGMEM_STRING_TABLE(UnsetValues, "None", "unavailable", "unknown");

bool is_unset(StringRef value) {
  for (uint8_t i = 0; i < UnsetValues::COUNT; i++) {
    const char *entry = UnsetValues::get_(i, i);
    if (ESPHOME_strlen_P(entry) == value.size() && ESPHOME_strncmp_P(value.c_str(), entry, value.size()) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace esphome::homeassistant
