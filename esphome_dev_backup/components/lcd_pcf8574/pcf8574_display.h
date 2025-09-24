#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lcd_base/lcd_display.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lcd_pcf8574 {

class PCF8574LCDDisplay : public lcd_base::LCDDisplay, public i2c::I2CDevice {
 public:
  void set_writer(std::function<void(PCF8574LCDDisplay &)> &&writer) { this->writer_ = std::move(writer); }
  void setup() override;
  void dump_config() override;
  void backlight();
  void no_backlight();

 protected:
  bool is_four_bit_mode() override { return true; }
  void write_n_bits(uint8_t value, uint8_t n) override;
  void send(uint8_t value, bool rs) override;

  void call_writer() override { this->writer_(*this); }

  // Stores the current state of the backlight.
  uint8_t backlight_value_;
  std::function<void(PCF8574LCDDisplay &)> writer_;
};

}  // namespace lcd_pcf8574
}  // namespace esphome
