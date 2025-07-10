#pragma once
#include <stdint.h>

namespace esphome {

enum class EntityType : uint8_t {
  NONE,
  BINARY_SENSOR,
  SWITCH,
  SENSOR,
};

}  // namespace esphome
