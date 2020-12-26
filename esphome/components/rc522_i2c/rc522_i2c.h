#pragma once

#include "esphome/core/component.h"
#include "esphome/components/rc522/rc522.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace rc522_i2c {

class RC522I2C : public rc522::RC522, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  bool write_data(const std::vector<uint8_t> &data) override;
  bool read_data(std::vector<uint8_t> &data, uint8_t len) override;
};

}  // namespace rc522_i2c
}  // namespace esphome
