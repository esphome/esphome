#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../sx126x.h"

namespace esphome::sx126x {

// The noise floor: the median of the channel power, sampled while the radio listens with no packet arriving, over
// each update interval. Samples are taken at the sample interval and kept in a buffer allocated once at setup.
class SX126xSensor : public PollingComponent, public Parented<SX126x> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_noise_floor_sensor(sensor::Sensor *sensor) { this->noise_floor_sensor_ = sensor; }
  void set_sample_interval(uint32_t interval_ms) { this->sample_interval_ms_ = interval_ms; }
  void set_max_samples(size_t max_samples) { this->max_samples_ = max_samples; }

 protected:
  void sample_();

  sensor::Sensor *noise_floor_sensor_{nullptr};
  // In half dB, the radio's resolution
  FixedVector<int16_t> samples_;
  size_t max_samples_{0};
  uint32_t sample_interval_ms_{100};
};

}  // namespace esphome::sx126x
