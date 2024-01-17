#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn7160/pn7160.h"
#include "esphome/components/i2c/i2c.h"

#include <vector>

namespace esphome {
namespace pn7160_i2c {

class PN7160I2C : public pn7160::PN7160, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  uint8_t read_nfcc(nfc::NciMessage &rx, uint16_t timeout) override;
  uint8_t write_nfcc(nfc::NciMessage &tx) override;
};

}  // namespace pn7160_i2c
}  // namespace esphome
