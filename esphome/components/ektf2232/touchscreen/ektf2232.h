#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ektf2232 {

using namespace touchscreen;

class EKTF2232Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

  void set_power_state(bool enable);
  bool get_power_state();

 protected:
  void hard_reset_();
  bool soft_reset_();
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_;
};

}  // namespace ektf2232
}  // namespace esphome
