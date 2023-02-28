#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/i2c/i2c.h"

#include <vector>

namespace esphome {
namespace pn532_i2c {

class PN532I2C : public pn532::PN532, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  bool write_data(const std::vector<uint8_t> &data) override;
  bool read_data(std::vector<uint8_t> &data, uint8_t len) override;
  bool read_response(uint8_t command, std::vector<uint8_t> &data) override;
  uint8_t read_response_length_();
};

}  // namespace pn532_i2c
}  // namespace esphome
