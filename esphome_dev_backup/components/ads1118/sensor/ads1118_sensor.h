#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../ads1118.h"

namespace esphome {
namespace ads1118 {

class ADS1118Sensor : public PollingComponent,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler,
                      public Parented<ADS1118> {
 public:
  void update() override;

  void set_multiplexer(ADS1118Multiplexer multiplexer) { this->multiplexer_ = multiplexer; }
  void set_gain(ADS1118Gain gain) { this->gain_ = gain; }
  void set_temperature_mode(bool temp) { this->temperature_mode_ = temp; }

  float sample() override;

  void dump_config() override;

 protected:
  ADS1118Multiplexer multiplexer_{ADS1118_MULTIPLEXER_P0_NG};
  ADS1118Gain gain_{ADS1118_GAIN_6P144};
  bool temperature_mode_;
};

}  // namespace ads1118
}  // namespace esphome
