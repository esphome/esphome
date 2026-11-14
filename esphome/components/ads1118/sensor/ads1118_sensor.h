#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../ads1118.h"

namespace esphome::ads1118 {

class ADS1118Sensor final : public PollingComponent,
                            public sensor::Sensor,
                            public voltage_sampler::VoltageSampler,
                            public Parented<ADS1118> {
 public:
  explicit ADS1118Sensor(uint16_t config_bits) : config_bits_(config_bits) {}
  void update() override;

  float sample() override;

  void dump_config() override;

 protected:
  const uint16_t config_bits_;
};

}  // namespace esphome::ads1118
