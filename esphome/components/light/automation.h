#pragma once

#include "esphome/core/automation.h"
#include "light_state.h"
#include "addressable_light.h"
#include "light_color_values.h"

namespace esphome::light {

enum class LimitMode { CLAMP, DO_NOTHING };

/// Set the state and brightness of a light.dim_relative call on `state`. Returns false, and the call must not be
/// performed, when `limit_mode` is DO_NOTHING and the current brightness is outside the limits.
bool dim_relative(LightState *state, LightCall &call, float relative_brightness, float min_brightness = 0.0f,
                  float max_brightness = 1.0f, LimitMode limit_mode = LimitMode::CLAMP);

/// Turn on the next or previous configured effect; with `include_none`, "None" is part of the cycle.
void cycle_effect(LightState *state, bool forward, bool include_none);

/// Codegen use only: whether the light turned on since the report one on_turn_on automation last saw, kept in `last`.
inline bool update_turned_on(bool &last, bool now) {
  const bool turned_on = now && !last;
  last = now;
  return turned_on;
}

#ifdef USE_LIGHT_MOVE
/// light.move on brightness: toward max_brightness (up) or min_brightness (down) at a constant speed that crosses
/// 0-100% in full_range_ms; 0 or less changes at once. Up turns an off light on at min_brightness, and continues from
/// where a light that is fading out is. Down leaves an off light off. With a min_brightness of 0, the light stays on
/// until it gets there and then turns off, as Matter's MoveWithOnOff does. Like a transition, the call publishes its
/// target, the limit, as the state at once; light.stop publishes where the light got to. A move to 0% is not saved
/// when it starts, only when it ends or stops.
/// A move whose limit the light is at already stops a running move of brightness instead, so the light does not go on
/// the other way; a running move of color temperature is not affected.
/// A move that takes over a running transition holds the color temperature, or the cold and warm white channels, where
/// they are shown, so they do not move on or jump at the stop. RGB is not held: it keeps the target of the call.
/// Limits:
/// - An addressable light without an effect gets one color on all LEDs, the same end state as any turn_on or
///   dim_relative, without the per-pixel fade of its own transition.
/// - A light of the `homeassistant` platform shows nothing of a held move until the release: that platform drops writes
///   while a transition runs, and a move is one.
void move_brightness(LightState *state, bool up, int32_t full_range_ms, float min_brightness, float max_brightness);
/// light.move on color temperature: UP IS WARMER (more mireds), toward the light's max_mireds, and down is colder,
/// toward its min_mireds, at a constant speed that crosses the whole range in full_range_ms; 0 or less changes at
/// once. Only for a light that is on, in a mode with color temperature. It holds the brightness where it is shown, and
/// stops a running move of color temperature, not of brightness, when the light is at the limit already.
void move_color_temperature(LightState *state, bool up, int32_t full_range_ms);
#endif

template<typename... Ts> class AddressableSet final : public Action<Ts...> {
 public:
  explicit AddressableSet(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, range_from)
  TEMPLATABLE_VALUE(int32_t, range_to)
  TEMPLATABLE_VALUE(float, color_brightness)
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(float, white)

  void play(const Ts &...x) override {
    auto *out = (AddressableLight *) this->parent_->get_output();
    const int32_t size = out->size();
    int32_t range_from = interpret_index(this->range_from_.value_or(x..., 0), size);
    if (range_from < 0 || range_from >= size)
      range_from = 0;

    int32_t range_to = interpret_index(this->range_to_.value_or(x..., size - 1) + 1, size);
    if (range_to < 0 || range_to >= size)
      range_to = size;

    uint8_t color_brightness =
        to_uint8_clamped(this->color_brightness_.value_or(x..., this->parent_->remote_values.get_color_brightness()));
    auto range = out->range(range_from, range_to);
    if (this->red_.has_value())
      range.set_red(esp_scale8(to_uint8_clamped(this->red_.value(x...)), color_brightness));
    if (this->green_.has_value())
      range.set_green(esp_scale8(to_uint8_clamped(this->green_.value(x...)), color_brightness));
    if (this->blue_.has_value())
      range.set_blue(esp_scale8(to_uint8_clamped(this->blue_.value(x...)), color_brightness));
    if (this->white_.has_value())
      range.set_white(to_uint8_clamped(this->white_.value(x...)));
    out->schedule_show();
  }

 protected:
  LightState *parent_;

  static inline int32_t interpret_index(int32_t index, int32_t size) { return index < 0 ? index + size : index; }

  static inline uint8_t to_uint8_clamped(float value) { return to_uint8_scale(clamp_unit_float(value)); }
};

}  // namespace esphome::light
