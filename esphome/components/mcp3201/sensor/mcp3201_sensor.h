#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "../mcp3201.h"

namespace esphome {
namespace mcp3201 {

class MCP3201Sensor : public PollingComponent,
                      public Parented<MCP3201>,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler {
 public:
  MCP3201Sensor() = default;

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;
};

}  // namespace mcp3201
}  // namespace esphome
