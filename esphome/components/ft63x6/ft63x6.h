/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ft63x6 {

using namespace touchscreen;

class FT63X6Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void hard_reset_();
  uint8_t read_byte_(uint8_t addr);
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};

  uint8_t read_touch_count_();
  uint16_t read_touch_coordinate_(uint8_t coordinate);
  uint8_t read_touch_id_(uint8_t id_address);
};

}  // namespace ft63x6
}  // namespace esphome
