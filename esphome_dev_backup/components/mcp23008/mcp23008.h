#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23x08_base/mcp23x08_base.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp23008 {

class MCP23008 : public mcp23x08_base::MCP23X08Base, public i2c::I2CDevice {
 public:
  MCP23008() = default;

  void setup() override;
  void dump_config() override;

 protected:
  bool read_reg(uint8_t reg, uint8_t *value) override;
  bool write_reg(uint8_t reg, uint8_t value) override;
};

}  // namespace mcp23008
}  // namespace esphome
