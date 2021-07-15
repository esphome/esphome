#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4728 {

extern const uint8_t MCP4728_MULTI_IR_CMD;
extern const uint8_t MCP4728_MULTI_EEPROM_CMD;
extern const uint8_t MCP4728_FAST_WRITE_CMD;

typedef enum pd_mode {
  MCP4728_PD_MODE_NORMAL,
  MCP4728_PD_MODE_GND_1K,
  MCP4728_PD_MODE_GND_100K,
  MCP4728_PD_MODE_GND_500K
} MCP4728_pd_mode_t;

typedef enum gain { MCP4728_GAIN_1X, MCP4728_GAIN_2X } MCP4728_gain_t;

typedef enum vref { MCP4728_VREF_VDD, MCP4728_VREF_INTERNAL } MCP4728_vref_t;

typedef enum channel { MCP4728_CHANNEL_A, MCP4728_CHANNEL_B, MCP4728_CHANNEL_C, MCP4728_CHANNEL_D } MCP4728_channel_t;

class MCP4728OutputComponent;
class MCP4728Channel : public output::FloatOutput {
 public:
  MCP4728Channel(MCP4728OutputComponent *parent, uint8_t channel) : parent_(parent), channel_(channel) {}

 protected:
  void write_state(float state) override;

  MCP4728OutputComponent *parent_;
  uint8_t channel_;
};

class MCP4728OutputComponent : public Component, public i2c::I2CDevice {
 public:
  MCP4728OutputComponent() {}
  MCP4728Channel *create_channel(uint8_t channel);

  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  friend MCP4728Channel;
  bool set_channel_value_(MCP4728_channel_t channel, uint16_t new_value, MCP4728_vref_t new_vref,
                          MCP4728_gain_t new_gain, MCP4728_pd_mode_t new_pd_mode, bool udac);

  float value_;
};

}  // namespace mcp4728
}  // namespace esphome
