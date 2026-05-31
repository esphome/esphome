#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome::mcp47a1 {

class MCP47A1 : public Component, public output::FloatOutput, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void write_state(float state) override;
};

}  // namespace esphome::mcp47a1
