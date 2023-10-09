#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ade7953_base/ade7953_base.h"

#include <vector>

namespace esphome {
namespace ade7953_i2c {

class AdE7953I2c : public ade7953_base::ADE7953, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  bool ade_write_8(uint16_t reg, uint8_t value) override;
  bool ade_write_16(uint16_t reg, uint16_t value) override;
  bool ade_write_32(uint16_t reg, uint32_t value) override;
  bool ade_read_8(uint16_t reg, uint8_t *value) override;
  bool ade_read_16(uint16_t reg, uint16_t *value) override;
  bool ade_read_32(uint16_t reg, uint32_t *value) override;
};

}  // namespace ade7953_i2c
}  // namespace esphome
