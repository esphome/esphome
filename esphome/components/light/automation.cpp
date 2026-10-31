#include "automation.h"
#include "esphome/core/log.h"

namespace esphome::light {

ESPHOME_LOG_TAG(TAG, "light.automation");

bool dim_relative(LightState *state, LightCall &call, float relative_brightness, float min_brightness,
                  float max_brightness, LimitMode limit_mode) {
  float cur;
  state->remote_values.as_brightness(&cur);
  if ((limit_mode == LimitMode::DO_NOTHING) && ((cur < min_brightness) || (cur > max_brightness))) {
    return false;
  }
  float new_brightness = clamp(cur + relative_brightness, min_brightness, max_brightness);
  call.set_state(new_brightness != 0.0f);
  call.set_brightness(new_brightness);
  return true;
}

void cycle_effect(LightState *state, bool forward, bool include_none) {
  size_t count = state->get_effect_count();
  if (count == 0) {
    return;
  }
  uint32_t current = state->get_current_effect_index();
  uint32_t next;
  if (include_none) {
    uint32_t total = static_cast<uint32_t>(count) + 1;
    next = forward ? (current + 1) % total : (current + total - 1) % total;
  } else if (forward) {
    next = (current % static_cast<uint32_t>(count)) + 1;
  } else {
    next = (current <= 1) ? static_cast<uint32_t>(count) : current - 1;
  }
  auto call = state->turn_on();
  call.set_effect(next);
  call.perform();
}

void addressableset_warn_about_scale(const char *field) {
  ESP_LOGW(TAG, "Lambda for parameter %s of light.addressable_set should return values in range 0-1 instead of 0-255.",
           field);
}

}  // namespace esphome::light
