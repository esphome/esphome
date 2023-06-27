#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp4728 {

enum class CMD {
  FAST_WRITE = 0x00,
  MULTI_WRITE = 0x40,
  SINGLE_WRITE = 0x58,
  SEQ_WRITE = 0x50,
  SELECT_VREF = 0x80,
  SELECT_GAIN = 0xC0,
  SELECT_POWER_DOWN = 0xA0
};

enum MCP4728Vref { MCP4728_VREF_VDD = 0, MCP4728_VREF_INTERNAL_2_8V = 1 };

enum MCP4728PwrDown {
  MCP4728_PD_NORMAL = 0,
  MCP4728_PD_GND_1KOHM = 1,
  MCP4728_PD_GND_100KOHM = 2,
  MCP4728_PD_GND_500KOHM = 3
};

enum MCP4728Gain { MCP4728_GAIN_X1 = 0, MCP4728_GAIN_X2 = 1 };

enum MCP4728ChannelIdx { MCP4728_CHANNEL_A = 0, MCP4728_CHANNEL_B = 1, MCP4728_CHANNEL_C = 2, MCP4728_CHANNEL_D = 3 };

struct DACInputData {
  MCP4728Vref vref;
  MCP4728PwrDown pd;
  MCP4728Gain gain;
  uint16_t data;
};

class MCP4728Channel;

/// MCP4728 float output component.
class MCP4728Component : public Component, public i2c::I2CDevice {
 public:
  MCP4728Component(bool store_in_eeprom) : store_in_eeprom_(store_in_eeprom) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

 protected:
  friend MCP4728Channel;
  void set_channel_value_(MCP4728ChannelIdx channel, uint16_t value);
  bool multi_write_();
  bool seq_write_();
  void select_vref_(MCP4728ChannelIdx channel, MCP4728Vref vref);
  void select_power_down_(MCP4728ChannelIdx channel, MCP4728PwrDown pd);
  void select_gain_(MCP4728ChannelIdx channel, MCP4728Gain gain);

 private:
  DACInputData reg_[4];
  bool store_in_eeprom_ = false;
  bool update_ = false;
};

class MCP4728Channel : public output::FloatOutput {
 public:
  MCP4728Channel(MCP4728Component *parent, MCP4728ChannelIdx channel, MCP4728Vref vref, MCP4728Gain gain,
                 MCP4728PwrDown pwrdown)
      : parent_(parent), channel_(channel), vref_(vref), gain_(gain), pwrdown_(pwrdown) {
    // update VREF
    parent->select_vref_(channel, vref_);
    // update PD
    parent->select_power_down_(channel, pwrdown_);
    // update GAIN
    parent->select_gain_(channel, gain_);
  }

 protected:
  void write_state(float state) override;

  MCP4728Component *parent_;
  MCP4728ChannelIdx channel_;
  MCP4728Vref vref_;
  MCP4728Gain gain_;
  MCP4728PwrDown pwrdown_;
};

}  // namespace mcp4728
}  // namespace esphome
