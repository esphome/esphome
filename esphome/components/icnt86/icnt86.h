#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace icnt86 {

class ICNT86Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void reset_();
  void i2c_read_byte_(uint16_t reg, char const *data, uint8_t len);
  void icnt_read_(uint16_t reg, char const *data, uint8_t len);
  void icnt_write_(uint16_t reg, char const *data, uint8_t len);
  void i2c_write_byte_(uint16_t reg, char const *data, uint8_t len);
  void reset_touch_sensor_();
  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{nullptr};

  uint16_t x_old_ = 0;
  uint16_t y_old_ = 0;
  bool p_old_zero_ = false;
};

}  // namespace icnt86
}  // namespace esphome
