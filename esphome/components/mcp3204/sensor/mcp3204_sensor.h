#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "../mcp3204.h"

namespace esphome {
namespace mcp3204 {

class MCP3204Sensor : public PollingComponent,
                      public Parented<MCP3204>,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler {
 public:
  MCP3204Sensor(uint8_t pin, bool differential_mode) : pin_(pin), differential_mode_(differential_mode) {}

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  uint8_t pin_;
  bool differential_mode_;
};

}  // namespace mcp3204
}  // namespace esphome
