#include "automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::light {

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

#ifdef USE_LIGHT_MOVE
ESPHOME_LOG_TAG(TAG, "light");

// `start` is where the light is now (get_shown_values()), not remote_values, which already holds the target of a
// running transition. The brightness range of 100% takes full_range_ms to cross.
static void move_to(LightState *state, LightCall &call, const LightColorValues &start, float from, float target,
                    int32_t full_range_ms) {
  if (full_range_ms <= 0) {
    ESP_LOGD(TAG, "'%s': light.move with a duration of %d ms changes at once", LOG_STR_ARG(state->get_log_name()),
             static_cast<int>(full_range_ms));
    call.set_transition_length(0).perform();
    return;
  }
  // At most 2^31 ms, as the duration is an int32_t. At least 1 ms, so the call makes a transition that
  // stop_move() can freeze.
  const auto length = static_cast<uint32_t>(std::fabs(target - from) * static_cast<float>(full_range_ms));
  call.set_transition_length(std::max<uint32_t>(length, 1));
  state->perform_move(call, start);
}

void move_brightness(LightState *state, bool up, int32_t full_range_ms, float min_brightness, float max_brightness) {
  if (!state->get_traits().supports_color_capability(ColorCapability::BRIGHTNESS)) {
    ESP_LOGW(TAG, "'%s': light.move needs brightness control", LOG_STR_ARG(state->get_log_name()));
    return;
  }
  const bool wants_on = state->remote_values.is_on();
  if (!wants_on && !up)
    return;  // off, or fading out
  // A light fading out keeps its brightness, one that is off starts at the minimum
  LightColorValues start = state->get_shown_values();
  const bool shown_on = start.is_on();
  if (!shown_on) {
    start = state->remote_values;
    start.set_state(true);
    start.set_brightness(min_brightness);
  }
  if (shown_on) {
    // The shown state may be a fraction (the second half of an interlocked RGB to white transition fades it in): fold
    // it into the brightness, so the move starts at what is shown and does not fade the state in on the way
    start.set_brightness(start.get_brightness() * start.get_state());
    start.set_state(true);
  }
  const float from = start.get_brightness();
  const float target = up ? max_brightness : min_brightness;
  if (up ? from >= target : from <= target) {
    // A running move of brightness the other way would go on, as the light is at the limit of this one
    if (state->is_moving())
      state->stop_move();
    return;
  }
  auto call = state->make_call();
  // A move to 0% needs an explicit state: it stays on until it gets there. A change at once has no there, and a
  // brightness of 0 alone turns the light off.
  if (full_range_ms > 0 || target > 0.0f)
    call.set_state(true);
  call.set_brightness(target);
  // A running transition would go on to its end, and a stop would jump there, as it replays only the brightness and
  // the rest comes from remote_values, the target of the transition: hold the white attributes and the color brightness
  // where they are shown. RGB is not held: the call renormalises it, so it keeps the call's target, as it does in any
  // other call.
  const ColorMode mode = start.get_color_mode();
  if (shown_on && state->is_transitioning()) {
    if ((mode & ColorCapability::COLOR_TEMPERATURE) || (mode & ColorCapability::COLD_WARM_WHITE)) {
      call.set_color_temperature(start.get_color_temperature());
      if (!(mode & ColorCapability::COLOR_TEMPERATURE))
        call.set_color_mode(mode).set_cold_white(start.get_cold_white()).set_warm_white(start.get_warm_white());
    }
    if (mode & ColorCapability::WHITE)
      call.set_white(start.get_white());
    // Setting the white, the color brightness or the channels also holds the color mode shown: a change between RGB and
    // white that runs is dropped, and the move stays in the mode shown
    if (mode & ColorCapability::RGB)
      call.set_color_brightness(start.get_color_brightness());
  }
  // A timed move to 0% is saved when it ends or stops: a reset on the way restores the state before it, not on at 0%
  if (target == 0.0f && full_range_ms > 0)
    call.set_save(false);
  move_to(state, call, start, from, target, full_range_ms);
}
#endif

}  // namespace esphome::light
