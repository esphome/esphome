#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mcp23x17_base/mcp23x17_base.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp23017 {

class MCP23017 : public mcp23x17_base::MCP23X17Base, public i2c::I2CDevice {
 public:
  MCP23017() = default;

  void setup() override;
  void dump_config() override;

 protected:
  bool read_reg(uint8_t reg, uint8_t *value) override;
  bool write_reg(uint8_t reg, uint8_t value) override;
};

}  // namespace mcp23017
}  // namespace esphome
