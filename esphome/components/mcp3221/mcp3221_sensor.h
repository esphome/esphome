#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include <cinttypes>

namespace esphome {
namespace mcp3221 {

class MCP3221Sensor : public sensor::Sensor,
                      public PollingComponent,
                      public voltage_sampler::VoltageSampler,
                      public i2c::I2CDevice {
 public:
  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }
  void update() override;
  float sample() override;

 protected:
  float reference_voltage_;
};

}  // namespace mcp3221
}  // namespace esphome
