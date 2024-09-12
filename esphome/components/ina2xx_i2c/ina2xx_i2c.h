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
  bool read_ina_register(uint8_t reg, uint8_t *data, size_t len) override;
  bool write_ina_register(uint8_t reg, const uint8_t *data, size_t len) override;
};

}  // namespace ina2xx_i2c
}  // namespace esphome
