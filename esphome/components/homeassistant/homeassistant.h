#pragma once

#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/// True for what Home Assistant reports when there is no value: "None" for an attribute that is not
/// set, "unavailable" and "unknown" for the state of an entity that is not ready
bool is_unset(StringRef value);

}  // namespace esphome::homeassistant
