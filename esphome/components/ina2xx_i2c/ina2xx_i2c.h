#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ina2xx_base/ina2xx_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ina2xx_i2c {

class INA2XXI2C : public ina2xx_base::INA2XX, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};

  bool read_2xx(uint8_t a_register, uint8_t *data, size_t len) override;
  bool write_2xx(uint8_t a_register, const uint8_t *data, size_t len) override;
};

}  // namespace ina2xx_i2c
}  // namespace esphome
