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

static const uint32_t MEASUREMENT_TIME_12BIT_MS = 5;
static const uint32_t MEASUREMENT_TIME_14BIT_MS = 17;
static const uint32_t MEASUREMENT_TIME_16BIT_MS = 67;

class MCP3428Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_continuous_mode(bool continuous_mode) { continuous_mode_ = continuous_mode; }

  // Helper method to request a measurement from a sensor. Returns true if measurement is started and false if sensor is
  // busy. Due to asyncronous measurement will return a best guess as to the necessary wait time for either request
  // retry or polling.
  bool request_measurement(MCP3428Multiplexer multiplexer, MCP3428Gain gain, MCP3428Resolution resolution,
                           uint32_t &timeout_wait);
  // poll component for a measurement. Returns true if value is available and sets voltage to the result.
  bool poll_result(float &voltage);

  void abandon_current_measurement() { single_measurement_active_ = false; }

 protected:
  float convert_answer_to_voltage_(uint8_t const *answer);

  uint8_t prev_config_{0};
  uint32_t last_config_write_ms_{0};
  bool continuous_mode_;

  bool single_measurement_active_;
};

}  // namespace mcp3428
}  // namespace esphome
