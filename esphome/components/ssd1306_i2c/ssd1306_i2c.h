#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ssd1306_base/ssd1306_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ssd1306_i2c {

class I2CSSD1306 : public ssd1306_base::SSD1306, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void command(uint8_t value) override;
  void write_display_data() override;

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};
};

}  // namespace ssd1306_i2c
}  // namespace esphome
