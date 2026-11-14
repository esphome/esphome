#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../ads1115.h"

namespace esphome::ads1115 {

class ADS1115Sensor final : public sensor::Sensor,
                            public PollingComponent,
                            public voltage_sampler::VoltageSampler,
                            public Parented<ADS1115Component> {
 public:
  ADS1115Sensor(uint16_t config_bits, uint8_t conversion_delay_ms)
      : config_bits_(config_bits), conversion_delay_ms_(conversion_delay_ms) {}
  void update() override;
  float sample() override;

  void dump_config() override;

 protected:
  const uint16_t config_bits_;
  const uint8_t conversion_delay_ms_;
};

}  // namespace esphome::ads1115
