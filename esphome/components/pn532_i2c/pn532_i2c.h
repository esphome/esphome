#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/i2c/i2c.h"

#include <span>

namespace esphome::pn532_i2c {

class PN532I2C final : public pn532::PN532, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  bool is_read_ready() override;
  bool write_data(std::span<const uint8_t> data) override;
  bool read_data(pn532::PN532Frame &data, size_t len) override;
  bool read_response(uint8_t command, pn532::PN532Frame &data) override;
  uint8_t read_response_length_();
};

}  // namespace esphome::pn532_i2c
