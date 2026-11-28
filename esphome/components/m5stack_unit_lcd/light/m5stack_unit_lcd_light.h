#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/m5stack_unit_lcd/m5stack_unit_lcd.h"
#include "esphome/core/helpers.h"

namespace esphome::m5stack_unit_lcd {

/// Brightness-only light that drives the Unit LCD's backlight through the display's BRIGHTNESS command.
class M5StackUnitLCDLight : public light::LightOutput, public Parented<M5StackUnitLCD> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void write_state(light::LightState *state) override {
    float brightness;
    state->current_values_as_brightness(&brightness);
    this->parent_->set_brightness(brightness);
  }
};

}  // namespace esphome::m5stack_unit_lcd
