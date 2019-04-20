#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lcd_base/lcd_display.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lcd_pcf8574 {

class PCF8574LCDDisplay : public lcd_base::LCDDisplay, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  bool is_four_bit_mode() override { return true; }
  void write_n_bits(uint8_t value, uint8_t n) override;
  void send(uint8_t value, bool rs) override;
};

}  // namespace lcd_pcf8574
}  // namespace esphome
