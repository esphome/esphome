#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::mcp4725 {

static constexpr uint8_t MCP4725_ADDR = 0x60;
static constexpr uint8_t MCP4725_RES = 12;

class MCP4725 final : public Component, public output::FloatOutput, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;

 protected:
  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};
};

}  // namespace esphome::mcp4725
