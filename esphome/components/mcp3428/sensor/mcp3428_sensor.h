#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../mcp3428.h"

namespace esphome {
namespace mcp3428 {

/// Internal holder class that is in instance of Sensor so that the hub can create individual sensors.
class MCP3428Sensor : public sensor::Sensor,
                      public PollingComponent,
                      public voltage_sampler::VoltageSampler,
                      public Parented<MCP3428Component> {
 public:
  void update() override;
  void set_multiplexer(MCP3428Multiplexer multiplexer) { this->multiplexer_ = multiplexer; }
  void set_gain(MCP3428Gain gain) { this->gain_ = gain; }
  void set_resolution(MCP3428Resolution resolution) { this->resolution_ = resolution; }
  // the sample function should ONLY be used when one channel is read in continuous mode or 12 bit conversion is used as
  // it blocks!
  float sample() override;

  void dump_config() override;

 protected:
  int initial_measurement_request_ms_;
  MCP3428Multiplexer multiplexer_;
  MCP3428Gain gain_;
  MCP3428Resolution resolution_;
};

}  // namespace mcp3428
}  // namespace esphome
