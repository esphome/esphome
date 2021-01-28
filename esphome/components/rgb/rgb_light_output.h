#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgb {

static const char *TAG = "RGBlight";

class RGBLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(true);
    traits.set_supports_color_temperature(true);
    traits.set_min_mireds(100);
    traits.set_max_mireds(500);
    traits.set_supports_color_interlock(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue, false);

    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
    ESP_LOGD(TAG, "red: %.1f green: %.1f blue: %.1f", red, green, blue);
    //ESP_LOGD(TAG, "  Color Temperature: %.1f mireds", v.get_color_temperature());
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
};

}  // namespace rgb
}  // namespace esphome
