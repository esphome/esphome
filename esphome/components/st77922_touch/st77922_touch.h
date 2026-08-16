#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace st77922_touch {

class ST77922Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  bool read_reg16_(uint16_t reg, uint8_t *data, size_t len);
  void try_init_();

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint8_t max_points_{0};
  uint8_t init_retries_{0};
  bool ready_{false};
};

}  // namespace st77922_touch
}  // namespace esphome
