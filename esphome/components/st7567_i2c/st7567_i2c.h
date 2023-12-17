#pragma once

#include "esphome/core/component.h"
#include "esphome/components/st7567_base/st7567_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace st7567_i2c {

class I2CST7567 : public st7567_base::ST7567, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void command(uint8_t value) override;
  void write_display_data() override;

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};
};

}  // namespace st7567_i2c
}  // namespace esphome
