#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "../adc128s102.h"

namespace esphome {
namespace adc128s102 {

class ADC128S102Sensor : public PollingComponent,
                         public Parented<ADC128S102>,
                         public sensor::Sensor,
                         public voltage_sampler::VoltageSampler {
 public:
  ADC128S102Sensor(uint8_t channel);

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  uint8_t channel_;
};
}  // namespace adc128s102
}  // namespace esphome
