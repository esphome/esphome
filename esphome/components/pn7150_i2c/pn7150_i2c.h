#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn7150/pn7150.h"
#include "esphome/components/i2c/i2c.h"

#include <vector>

namespace esphome {
namespace pn7150_i2c {

class PN7150I2C : public pn7150::PN7150, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  uint8_t read_nfcc_(nfc::NciMessage &rx, const uint16_t timeout = pn7150::NFCC_DEFAULT_TIMEOUT) override;
  uint8_t write_nfcc_(nfc::NciMessage &tx) override;
};

}  // namespace pn7150_i2c
}  // namespace esphome
