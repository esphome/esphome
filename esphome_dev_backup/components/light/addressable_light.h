#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/color.h"
#include "esp_color_correction.h"
#include "esp_color_view.h"
#include "esp_range_view.h"
#include "light_output.h"
#include "light_state.h"
#include "transformers.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace light {

using ESPColor ESPDEPRECATED("esphome::light::ESPColor is deprecated, use esphome::Color instead.", "v1.21") = Color;

/// Convert the color information from a `LightColorValues` object to a `Color` object (does not apply brightness).
Color color_from_light_color_values(LightColorValues val);

/// Use a custom state class for addressable lights, to allow type system to discriminate between addressable and
/// non-addressable lights.
class AddressableLightState : public LightState {
  using LightState::LightState;
};

class AddressableLight : public LightOutput, public Component {
 public:
  virtual int32_t size() const = 0;
  ESPColorView operator[](int32_t index) const { return this->get_view_internal(interpret_index(index, this->size())); }
  ESPColorView get(int32_t index) { return this->get_view_internal(interpret_index(index, this->size())); }
  virtual void clear_effect_data() = 0;
  ESPRangeView range(int32_t from, int32_t to) {
    from = interpret_index(from, this->size());
    to = interpret_index(to, this->size());
    return ESPRangeView(this, from, to);
  }
  ESPRangeView all() { return ESPRangeView(this, 0, this->size()); }
  ESPRangeIterator begin() { return this->all().begin(); }
  ESPRangeIterator end() { return this->all().end(); }
  void shift_left(int32_t amnt) {
    if (amnt < 0) {
      this->shift_right(-amnt);
      return;
    }
    if (amnt > this->size())
      amnt = this->size();
    this->range(0, -amnt) = this->range(amnt, this->size());
  }
  void shift_right(int32_t amnt) {
    if (amnt < 0) {
      this->shift_left(-amnt);
      return;
    }
    if (amnt > this->size())
      amnt = this->size();
    this->range(amnt, this->size()) = this->range(0, -amnt);
  }
  // Indicates whether an effect that directly updates the output buffer is active to prevent overwriting
  bool is_effect_active() const { return this->effect_active_; }
  void set_effect_active(bool effect_active) { this->effect_active_ = effect_active; }
  std::unique_ptr<LightTransformer> create_default_transition() override;
  void set_correction(float red, float green, float blue, float white = 1.0f) {
    this->correction_.set_max_brightness(
        Color(to_uint8_scale(red), to_uint8_scale(green), to_uint8_scale(blue), to_uint8_scale(white)));
  }
  void setup_state(LightState *state) override {
    this->correction_.calculate_gamma_table(state->get_gamma_correct());
    this->state_parent_ = state;
  }
  void update_state(LightState *state) override;
  void schedule_show() { this->state_parent_->next_write_ = true; }

#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  void call_setup() override;

 protected:
  friend class AddressableLightTransformer;

  void mark_shown_() {
#ifdef USE_POWER_SUPPLY
    for (const auto &c : *this) {
      if (c.get_red_raw() > 0 || c.get_green_raw() > 0 || c.get_blue_raw() > 0 || c.get_white_raw() > 0) {
        this->power_.request();
        return;
      }
    }
    this->power_.unrequest();
#endif
  }
  virtual ESPColorView get_view_internal(int32_t index) const = 0;

  bool effect_active_{false};
  ESPColorCorrection correction_{};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif
  LightState *state_parent_{nullptr};
};

class AddressableLightTransformer : public LightTransitionTransformer {
 public:
  AddressableLightTransformer(AddressableLight &light) : light_(light) {}

  void start() override;
  optional<LightColorValues> apply() override;

 protected:
  AddressableLight &light_;
  Color target_color_{};
  float last_transition_progress_{0.0f};
  float accumulated_alpha_{0.0f};
};

}  // namespace light
}  // namespace esphome
