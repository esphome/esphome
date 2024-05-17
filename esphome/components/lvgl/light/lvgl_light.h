#pragma once

#if LVGL_USES_LIGHT
#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include <lvgl.h>

namespace esphome {
namespace lvgl {

class LVLight : public light::LightOutput {
 public:
  void set_control_lambda(std::function<void(lv_color_t)> lambda) {
    this->control_lambda_ = lambda;
    if (this->initial_value_) {
      this->control_lambda_(this->initial_value_.value());
      this->initial_value_.reset();
    }
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue, false);
    auto color = lv_color_make(red * 255, green * 255, blue * 255);
    if (this->control_lambda_ != nullptr) {
      this->control_lambda_(color);
    } else {
      this->initial_value_ = color;
    }
  }

 protected:
  std::function<void(lv_color_t)> control_lambda_{};
  optional<lv_color_t> initial_value_{};
};

}  // namespace lvgl
}  // namespace esphome

#endif
