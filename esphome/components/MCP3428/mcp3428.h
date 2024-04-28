#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mcp3428 {

// the second bit is ignored in MCP3426/7 and will result in measurement of channel 1 or 2
enum MCP3428Multiplexer {
  MCP3428_MULTIPLEXER_CHANNEL_1 = 0b00,
  MCP3428_MULTIPLEXER_CHANNEL_2 = 0b01,
  MCP3428_MULTIPLEXER_CHANNEL_3 = 0b10,
  MCP3428_MULTIPLEXER_CHANNEL_4 = 0b11,
};

enum MCP3428Gain {
  MCP3428_GAIN_1 = 0b00,
  MCP3428_GAIN_2 = 0b01,
  MCP3428_GAIN_4 = 0b10,
  MCP3428_GAIN_8 = 0b11,
};

enum MCP3428Resolution {
  MCP3428_12_BITS = 0b00,
  MCP3428_14_BITS = 0b01,
  MCP3428_16_BITS = 0b10,
};

class MCP3428Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_continuous_mode(bool continuous_mode) { continuous_mode_ = continuous_mode; }

  /// Helper method to request a measurement from a sensor.
  float request_measurement(MCP3428Multiplexer multiplexer, MCP3428Gain gain, MCP3428Resolution resolution);

 protected:
  uint8_t prev_config_{0};
  bool continuous_mode_;
};

}  // namespace mcp3428
}  // namespace esphome
