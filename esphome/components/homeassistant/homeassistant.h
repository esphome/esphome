#pragma once

#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/// The index of value in a PROGMEM_STRING_TABLE, or -1
template<typename Table> int8_t find_in_table(StringRef value) {
  for (uint8_t i = 0; i < Table::COUNT; i++) {
    const char *entry = Table::get_(i, i);
    if (ESPHOME_strlen_P(entry) == value.size() && ESPHOME_strncmp_P(value.c_str(), entry, value.size()) == 0) {
      return i;
    }
  }
  return -1;
}

/// True for what Home Assistant reports when there is no value: "None" for an attribute that is not
/// set, "unavailable" and "unknown" for the state of an entity that is not ready
bool is_unset(StringRef value);

}  // namespace esphome::homeassistant
