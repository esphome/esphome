#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "../lvgl_esphome.h"

namespace esphome {
namespace lvgl {

class LVLight : public light::LightOutput {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue, false);
    auto color = lv_color_make(red * 255, green * 255, blue * 255);
    if (this->obj_ != nullptr) {
      this->set_value_(color);
    } else {
      this->initial_value_ = color;
    }
  }

  void set_obj(lv_obj_t *obj) {
    this->obj_ = obj;
    if (this->initial_value_) {
      lv_led_set_color(obj, this->initial_value_.value());
      lv_led_on(obj);
      this->initial_value_.reset();
    }
  }

 protected:
  void set_value_(lv_color_t value) {
    lv_led_set_color(this->obj_, value);
    lv_led_on(this->obj_);
    lv_event_send(this->obj_, lv_api_event, nullptr);
  }
  lv_obj_t *obj_{};
  optional<lv_color_t> initial_value_{};
};

}  // namespace lvgl
}  // namespace esphome
