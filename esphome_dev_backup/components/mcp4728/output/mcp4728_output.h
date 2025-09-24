#pragma once

#include "../mcp4728.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp4728 {

class MCP4728Channel : public output::FloatOutput {
 public:
  MCP4728Channel(MCP4728Component *parent, MCP4728ChannelIdx channel, MCP4728Vref vref, MCP4728Gain gain,
                 MCP4728PwrDown pwrdown)
      : parent_(parent), channel_(channel) {
    // update VREF
    parent->select_vref_(channel, vref);
    // update PD
    parent->select_power_down_(channel, pwrdown);
    // update GAIN
    parent->select_gain_(channel, gain);
  }

 protected:
  void write_state(float state) override;

  MCP4728Component *parent_;
  MCP4728ChannelIdx channel_;
};

}  // namespace mcp4728
}  // namespace esphome
