#pragma once
#include <stdint.h>

namespace esphome {

enum class EntityType : uint8_t {
  NONE,
  SWITCH,
  SENSOR,
};

}  // namespace esphome
