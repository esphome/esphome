#pragma once
#include "fan_state.h"

namespace esphome {
namespace fan {

FanSpeed speed_level_to_enum(int speed_level, int supported_speed_levels);
int speed_enum_to_level(FanSpeed speed, int supported_speed_levels);

}  // namespace fan
}  // namespace esphome
