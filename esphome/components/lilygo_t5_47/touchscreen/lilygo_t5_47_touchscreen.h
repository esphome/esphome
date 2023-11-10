#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <vector>

namespace esphome {
namespace lilygo_t5_47 {

using namespace touchscreen;

class LilygoT547Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;

  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
};

}  // namespace lilygo_t5_47
}  // namespace esphome
