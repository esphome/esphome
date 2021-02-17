#include "fan_helpers.h"
#include "fan_state.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace fan {

int percentage_to_range(float percentage, int start, int end) {
  const int num_steps = end - start + 1;
  const int value = static_cast<int>(std::ceil(clamp(percentage, 0.0f, 1.0f) * num_steps));
  return value > 0 ? start + value - 1 : start;
}

float speed_percentage_from_state(fan::FanState* state, float low, float medium, float high) {
  switch (state->speed_mode_) {
    case fan::FAN_SPEED_MODE_PRESET:
      switch (state->speed) {
        case fan::FAN_SPEED_LOW:
          return low;
        case fan::FAN_SPEED_MEDIUM:
          return medium;
        case fan::FAN_SPEED_HIGH:
          return high;
      }
    case fan::FAN_SPEED_MODE_PERCENTAGE:
      return state->speed_percentage;
  }

  return 0.0f;
}

}  // namespace fan
}  // namespace esphome
