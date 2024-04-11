#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../ads1115.h"

namespace esphome {
namespace ads1115 {

/// Internal holder class that is in instance of Sensor so that the hub can create individual sensors.
class ADS1115Sensor : public sensor::Sensor,
                      public PollingComponent,
                      public voltage_sampler::VoltageSampler,
                      public Parented<ADS1115Component> {
 public:
  void update() override;
  void set_multiplexer(ADS1115Multiplexer multiplexer) { this->multiplexer_ = multiplexer; }
  void set_gain(ADS1115Gain gain) { this->gain_ = gain; }
  void set_resolution(ADS1115Resolution resolution) { this->resolution_ = resolution; }
  float sample() override;

  void dump_config() override;

 protected:
  ADS1115Multiplexer multiplexer_;
  ADS1115Gain gain_;
  ADS1115Resolution resolution_;
};

}  // namespace ads1115
}  // namespace esphome
