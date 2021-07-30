#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/color.h"
#include "esp_color_correction.h"
#include "esp_color_view.h"
#include "esp_range_view.h"
#include "light_output.h"
#include "light_state.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace light {

using ESPColor = Color;

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
  bool is_effect_active() const { return this->effect_active_; }
  void set_effect_active(bool effect_active) { this->effect_active_ = effect_active; }
  void write_state(LightState *state) override;
  void set_correction(float red, float green, float blue, float white = 1.0f) {
    this->correction_.set_max_brightness(
        Color(to_uint8_scale(red), to_uint8_scale(green), to_uint8_scale(blue), to_uint8_scale(white)));
  }
  void setup_state(LightState *state) override {
    this->correction_.calculate_gamma_table(state->get_gamma_correct());
    this->state_parent_ = state;
  }
  void schedule_show() { this->next_show_ = true; }

#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  void call_setup() override;

 protected:
  bool should_show_() const { return this->effect_active_ || this->next_show_; }
  void mark_shown_() {
    this->next_show_ = false;
#ifdef USE_POWER_SUPPLY
    for (auto c : *this) {
      if (c.get().is_on()) {
        this->power_.request();
        return;
      }
    }
    this->power_.unrequest();
#endif
  }
  virtual ESPColorView get_view_internal(int32_t index) const = 0;

  bool effect_active_{false};
  bool next_show_{true};
  ESPColorCorrection correction_{};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif
  LightState *state_parent_{nullptr};
  float last_transition_progress_{0.0f};
  float accumulated_alpha_{0.0f};
};

}  // namespace light
}  // namespace esphome
