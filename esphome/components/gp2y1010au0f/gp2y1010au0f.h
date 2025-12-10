#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/core/component.h"

namespace esphome {
namespace gp2y1010au0f {

class GP2Y1010AU0FSensor : public PollingComponent, public sensor::Sensor {
 public:
  void setup() override {}
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_adc_source(voltage_sampler::VoltageSampler *source) { this->source_ = source; }
  void set_voltage_multiplier(float voltage_multiplier) { this->voltage_multiplier_ = voltage_multiplier; }
  void set_voltage_offset(float voltage_offset) { this->voltage_offset_ = voltage_offset; }
  void set_sample_duration(uint32_t sample_duration) { this->sample_duration_ = sample_duration; }
  void set_led_output(output::BinaryOutput *led_output) { this->led_output_ = led_output; }
  void set_sample_wait_before(uint32_t wait) { this->sample_wait_before_ = wait; }
  void set_sample_wait_after(uint32_t wait) { this->sample_wait_after_ = wait; }  // NEW
  void set_sample_wait_off(uint32_t wait) { this->sample_wait_off_ = wait; }      // NEW

 protected:
  voltage_sampler::VoltageSampler *source_{nullptr};
  output::BinaryOutput *led_output_{nullptr};

  float voltage_multiplier_{1.0f};
  float voltage_offset_{0.0f};

  uint32_t sample_duration_{0};
  uint32_t sample_wait_before_{280};  // Wait before sampling (microseconds) - per datasheet
  uint32_t sample_wait_after_{40};    // Wait after sampling (microseconds) - per datasheet (NEW)
  uint32_t sample_wait_off_{9680};    // Wait after LED off (microseconds) - per datasheet (NEW)

  bool is_sampling_{false};
  uint32_t num_samples_{0};
  float sample_sum_{0.0f};
};

}  // namespace gp2y1010au0f
}  // namespace esphome
