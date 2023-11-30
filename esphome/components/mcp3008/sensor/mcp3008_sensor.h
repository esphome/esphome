#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"

#include "../mcp3008.h"

namespace esphome {
namespace mcp3008 {

class MCP3008Sensor : public PollingComponent,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler,
                      public Parented<MCP3008> {
 public:
  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  uint8_t pin_;
  float reference_voltage_;
};

}  // namespace mcp3008
}  // namespace esphome
