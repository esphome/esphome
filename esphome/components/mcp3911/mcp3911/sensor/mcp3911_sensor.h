#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "../mcp3911.h"

namespace esphome {
namespace mcp3911 {

class MCP3911Sensor : public PollingComponent,
                         public Parented<MCP3911>,
                         public sensor::Sensor,
                         public voltage_sampler::VoltageSampler {
 public:
  MCP3911Sensor(uint8_t channel);

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  uint8_t channel_;
};
}  // namespace adc128s102
}  // namespace esphome
