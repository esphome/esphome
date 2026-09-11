#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mipi_dsi {

class DsiBacklight : public light::LightOutput, public Component, public i2c::I2CDevice {
 public:
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_pwm_register(uint8_t pwm_register) { this->pwm_register_ = pwm_register; }

 protected:
  uint8_t brightness_{0xD0};
  uint8_t pwm_register_{0x86};
  bool inverted_{};
  bool setup_completed_{false};
};
}  // namespace mipi_dsi
}  // namespace esphome
