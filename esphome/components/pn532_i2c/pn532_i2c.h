#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pn532_i2c {

class PN532I2C : public pn532::PN532, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  std::vector<uint8_t> pn532_read_data_() override;

  bool is_ready() override;

  std::vector<uint8_t> pn532_read_bytes(uint8_t len) override {
    std::vector<uint8_t> data;
    this->read_bytes_raw(data.data(), len);
    return data;
  }

  void pn532_write_bytes(std::vector<uint8_t> data) override {
    this->write_bytes_raw(data);
  }
};

}  // namespace pn532_i2c
}  // namespace esphome
