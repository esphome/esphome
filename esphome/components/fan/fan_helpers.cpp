#include <cassert>
#include "fan_helpers.h"

namespace esphome {
namespace fan {

// This whole file is deprecated, don't warn about usage of deprecated types in here.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

FanSpeed speed_level_to_enum(int speed_level, int supported_speed_levels) {
  const auto speed_ratio = static_cast<float>(speed_level) / (supported_speed_levels + 1);
  const auto legacy_level = clamp<int>(static_cast<int>(ceilf(speed_ratio * 3)), 1, 3);
  return static_cast<FanSpeed>(legacy_level - 1);
}

int speed_enum_to_level(FanSpeed speed, int supported_speed_levels) {
  const auto enum_level = static_cast<int>(speed) + 1;
  const auto speed_level = roundf(enum_level / 3.0f * supported_speed_levels);
  return static_cast<int>(speed_level);
}

}  // namespace fan
}  // namespace esphome
