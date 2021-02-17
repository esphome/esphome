#pragma once

namespace esphome {
namespace fan {

class FanState;

/// Scale a percentage value to a specified integer range
int percentage_to_range(float percentage, int start, int end);

/// Get the speed in percentage from the fan state (used for backward compatibility)
float speed_percentage_from_state(fan::FanState* state, float low = 0.33f, float medium = 0.66f, float high = 1.0f);

}  // namespace fan
}  // namespace esphome
