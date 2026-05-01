#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace gp2y1010au0f {

class GP2Y1010AU0FSensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override {
    // after the base sensor has been initialized
    return setup_priority::DATA - 1.0f;
  }

  void set_adc_source(voltage_sampler::VoltageSampler *source) { source_ = source; }
  void set_voltage_refs(float offset, float multiplier) {
    this->voltage_offset_ = offset;
    this->voltage_multiplier_ = multiplier;
  }
  void set_led_output(output::BinaryOutput *output) { led_output_ = output; }

 protected:
  // duration in ms of the sampling phase
  uint32_t sample_duration_ = 100;
  // duration in us of the wait before sampling
  // ref: https://global.sharp/products/device/lineup/data/pdf/datasheet/gp2y1010au_appl_e.pdf
  uint32_t sample_wait_before_ = 280;
  // duration in us of the wait after sampling
  // it seems no need to delay on purpose since one ADC sampling takes longer than that (300-400 us on ESP8266)
  // uint32_t sample_wait_after_ = 40;
  // the sampling source to read voltage from
  voltage_sampler::VoltageSampler *source_;
  // ADC voltage reading offset
  float voltage_offset_ = 0.0f;
  // ADC voltage reading multiplier
  float voltage_multiplier_ = 1.0f;
  // the binary output to control the sampling LED
  output::BinaryOutput *led_output_;

  float sample_sum_ = 0.0f;
  uint32_t num_samples_ = 0;
  bool is_sampling_ = false;
};

}  // namespace gp2y1010au0f
}  // namespace esphome
