#pragma once
#include "fan_state.h"

namespace esphome {
namespace fan {

// Shut-up about usage of deprecated FanSpeed for a bit.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

ESPDEPRECATED("FanSpeed and speed_level_to_enum() are deprecated.", "2021.9")
FanSpeed speed_level_to_enum(int speed_level, int supported_speed_levels);
ESPDEPRECATED("FanSpeed and speed_enum_to_level() are deprecated.", "2021.9")
int speed_enum_to_level(FanSpeed speed, int supported_speed_levels);

#pragma GCC diagnostic pop

}  // namespace fan
}  // namespace esphome
