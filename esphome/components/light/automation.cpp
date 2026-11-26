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
// running transition. `from` and `target` are on a scale of `full_range` that takes full_range_ms to cross.
static void move_to(LightState *state, LightCall &call, const LightColorValues &start, bool brightness, float from,
                    float target, float full_range, int32_t full_range_ms) {
  if (full_range_ms <= 0) {
    ESP_LOGD(TAG, "'%s': light.move with a duration of %d ms changes at once", LOG_STR_ARG(state->get_log_name()),
             static_cast<int>(full_range_ms));
    call.set_transition_length(0).perform();
    return;
  }
  // At most 2^31 ms, as the duration is an int32_t. At least 1 ms, so the call makes a transition that
  // stop_move() can freeze.
  const auto length = static_cast<uint32_t>(std::fabs(target - from) / full_range * static_cast<float>(full_range_ms));
  call.set_transition_length(std::max<uint32_t>(length, 1));
  state->perform_move(call, start, brightness);
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
    if (state->is_moving(true))
      state->stop_move();
    return;
  }
  auto call = state->make_call();
  // A move to 0% needs an explicit state: it stays on until it gets there. A change at once has no there, and a
  // brightness of 0 alone turns the light off.
  if (full_range_ms > 0 || target > 0.0f)
    call.set_state(true);
  call.set_brightness(target);
  // A running transition or move of color temperature would go on to its end, and a stop would jump there, as it
  // replays only the brightness and the rest comes from remote_values, the target of the transition: hold the white
  // attributes and the color brightness where they are shown. A move's channels carry the scale it started with, so
  // they are held as they are. RGB is not held: the call renormalises it, so it keeps the call's target, as it does in
  // any other call.
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
  move_to(state, call, start, true, from, target, 1.0f, full_range_ms);
}

void move_color_temperature(LightState *state, bool up, int32_t full_range_ms) {
  // On as shown too: a turn-on that has not been applied yet is not a light that is on, and a move would ramp the
  // brightness of its target
  LightColorValues start = state->get_shown_values();
  if (!state->remote_values.is_on() || !start.is_on())
    return;
  // The mode the light shows, which is the target's once a transition between modes has got there
  const ColorMode mode = start.get_color_mode();
  if (mode != state->remote_values.get_color_mode())
    return;
  if (!(mode & ColorCapability::COLOR_TEMPERATURE) && !(mode & ColorCapability::COLD_WARM_WHITE)) {
    ESP_LOGW(TAG, "'%s': light.move needs a color mode with color temperature", LOG_STR_ARG(state->get_log_name()));
    return;
  }
  auto traits = state->get_traits();
  const float min_mireds = traits.get_min_mireds();
  const float max_mireds = traits.get_max_mireds();
  if (!std::isfinite(min_mireds) || !std::isfinite(max_mireds) || min_mireds <= 0.0f || max_mireds <= min_mireds) {
    ESP_LOGW(TAG, "'%s': light.move needs 0 < min_mireds < max_mireds", LOG_STR_ARG(state->get_log_name()));
    return;
  }
  // Up is more mireds (warmer), as MoveMode Up of the Move Color Temperature command in the Color Control cluster of
  // the Zigbee Cluster Library (07-5123) and the Matter Application Cluster specification
  const float target = up ? max_mireds : min_mireds;
  const float shown = state->get_shown_color_temperature(start, min_mireds, max_mireds);
  // Not a number: from the far end, the whole range
  const float from = std::isfinite(shown) ? clamp(shown, min_mireds, max_mireds) : (up ? min_mireds : max_mireds);
  start.set_color_temperature(from);
  // The shown state may be a fraction (the second half of an interlocked RGB to white transition fades it in): fold it
  // into the brightness, so the move holds what is shown, not the full brightness
  start.set_brightness(start.get_brightness() * start.get_state());
  start.set_state(true);
  if (up ? from >= target : from <= target) {
    if (state->is_moving(false))
      state->stop_move();
    return;
  }
  auto call = state->make_call();
  // Hold the brightness where it is shown, not at the target of a brightness move that runs; state, as a brightness of
  // 0 alone turns the light off
  call.set_state(true).set_brightness(start.get_brightness()).set_color_temperature(target);
  // A running change of white or of color brightness would go on to its target through the call, which inherits it:
  // hold the shown levels
  if (state->is_transitioning()) {
    if (mode & ColorCapability::WHITE)
      call.set_white(start.get_white());
    if (mode & ColorCapability::RGB)
      call.set_color_brightness(start.get_color_brightness());
  }
  // Channels set directly, below full, keep their scale (LightState::follow_color_temperature_() does on the way), so
  // the target has it too: the settled one of remote_values, not the shown, which dips halfway through a transition.
  // Dark channels stay dark. Channels of a color temperature are at full: nothing to set.
  if ((mode & ColorCapability::COLD_WARM_WHITE) && !(mode & ColorCapability::COLOR_TEMPERATURE)) {
    const float scale = state->get_channel_scale();
    if (scale < 1.0f) {
      float cold_white;
      float warm_white;
      state->color_temperature_to_cwww(target, min_mireds, max_mireds, &cold_white, &warm_white);
      call.set_color_mode(mode).set_cold_white(cold_white * scale).set_warm_white(warm_white * scale);
    }
  }
  move_to(state, call, start, false, from, target, max_mireds - min_mireds, full_range_ms);
}
#endif

}  // namespace esphome::light
