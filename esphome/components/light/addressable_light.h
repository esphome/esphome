#pragma once

#include "esp_color_buffer.h"
#include "esp_color_correction.h"
#include "esp_color_view.h"
#include "esp_range_view.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "light_output.h"
#include "light_state.h"
#include "light_transformer.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome::light {

/// Convert the color information from a `LightColorValues` object to a `Color` object (does not apply brightness).
Color color_from_light_color_values(LightColorValues val);

/// Use a custom state class for addressable lights, to allow type system to discriminate between addressable and
/// non-addressable lights.
class AddressableLightState final : public LightState {
  using LightState::LightState;
};

class AddressableLight : public LightOutput, public Component {
 public:
  /// Get the pixel data and effect data for this addressable light.
  virtual ESPColorBuffer &buffer() = 0;

  ESPDEPRECATED("Use buffer().size() instead. Will be removed in 2027.4.0.", "2026.10.0")
  int32_t size() { return this->buffer().size(); }

  ESPDEPRECATED("Use buffer()[index] instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPColorView operator[](int32_t index) { return this->buffer()[index]; }

  ESPDEPRECATED("Use buffer().get(index) instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPColorView get(int32_t index) { return this->buffer().get(index); }

  ESPDEPRECATED("Use buffer().clear_effect_data() instead. Will be removed in 2027.4.0.", "2026.10.0")
  void clear_effect_data() { this->buffer().clear_effect_data(); }

  ESPDEPRECATED("Use buffer().range(from, to) instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPRangeView range(int32_t from, int32_t to) { return this->buffer().range(from, to); }

  ESPDEPRECATED("Use buffer().all() instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPRangeView all() { return this->buffer().all(); }

  ESPDEPRECATED("Use buffer().begin() instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPRangeIterator begin() { return this->buffer().begin(); }

  ESPDEPRECATED("Use buffer().end() instead. Will be removed in 2027.4.0.", "2026.10.0")
  ESPRangeIterator end() { return this->buffer().end(); }

  ESPDEPRECATED("Use buffer().shift_left(amount) instead. Will be removed in 2027.4.0.", "2026.10.0")
  void shift_left(int32_t amount) { this->buffer().shift_left(amount); }

  ESPDEPRECATED("Use buffer().shift_right(amount) instead. Will be removed in 2027.4.0.", "2026.10.0")
  void shift_right(int32_t amount) { this->buffer().shift_right(amount); }

  // Indicates whether an effect that directly updates the output buffer is active to prevent overwriting
  bool is_effect_active() const { return this->effect_active_; }
  void set_effect_active(bool effect_active) { this->effect_active_ = effect_active; }
  std::unique_ptr<LightTransformer> create_default_transition() override;
  void set_correction(float red, float green, float blue, float white = 1.0f) {
    this->correction_.set_max_brightness(
        Color(to_uint8_scale(red), to_uint8_scale(green), to_uint8_scale(blue), to_uint8_scale(white)));
  }
  void setup_state(LightState *state) override {
#ifdef USE_LIGHT_GAMMA_LUT
    this->correction_.set_gamma_table(state->get_gamma_table());
#endif
    this->state_parent_ = state;
  }
  void update_state(LightState *state) override;
  void schedule_show() {
    if (this->state_parent_ != nullptr) {
      this->state_parent_->schedule_write_();
    }
  }

#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  void call_setup() override;

 protected:
  friend class AddressableLightTransformer;

  void mark_shown_() {
#ifdef USE_POWER_SUPPLY
    if (!this->buffer().is_all_black()) {
      this->power_.request();
    } else {
      this->power_.unrequest();
    }
#endif
  }

  ESPColorCorrection correction_{};
  LightState *state_parent_{nullptr};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif
  bool effect_active_{false};
};

class AddressableLightTransformer : public LightTransformer {
 public:
  AddressableLightTransformer(AddressableLight &light) : light_(light) {}

  void start() override;
  optional<LightColorValues> apply() override;

 protected:
  AddressableLight &light_;
  float last_transition_progress_{0.0f};
  Color target_color_{};
  Color uniform_start_color_{};
  bool uniform_start_scanned_{false};
  bool uniform_start_is_uniform_{false};
};

}  // namespace esphome::light
