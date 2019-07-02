#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace ct_clamp {

class CTClampSensor : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override {
    // After the base sensor has been initialized
    return setup_priority::DATA - 1.0f;
  }

  void set_sample_duration(uint32_t sample_duration) { sample_duration_ = sample_duration; }
  void set_source(voltage_sampler::VoltageSampler *source) { source_ = source; }

 protected:
  /// High Frequency loop() requester used during sampling phase.
  HighFrequencyLoopRequester high_freq_;

  /// Duration in ms of the sampling phase.
  uint32_t sample_duration_;
  /// The sampling source to read values from.
  voltage_sampler::VoltageSampler *source_;

  /** The DC offset of the circuit.
   *
   * Diagram: https://learn.openenergymonitor.org/electricity-monitoring/ct-sensors/interface-with-arduino
   *
   * This is automatically calculated with an exponential moving average/digital low pass filter.
   *
   * 0.5 is a good initial approximation to start with for most ESP8266 setups.
   */
  float offset_ = 0.5f;

  float sample_sum_ = 0.0f;
  uint32_t num_samples_ = 0;
  bool is_sampling_ = false;
  /// Calibrate offset value once at boot
  bool is_calibrating_offset_ = false;
};

}  // namespace ct_clamp
}  // namespace esphome
