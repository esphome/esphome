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
  /// Write the full command given in data to the PN532
  void pn532_write_command(const std::vector<uint8_t> &data) override;

  std::vector<uint8_t> pn532_read_data() override;

  bool is_ready() override;

  bool read_ack() override;
};

}  // namespace pn532_i2c
}  // namespace esphome
