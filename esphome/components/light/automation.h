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
