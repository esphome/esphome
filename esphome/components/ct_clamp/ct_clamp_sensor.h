#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace ct_clamp {

class CTClampSensor : public sensor::Sensor, public PollingComponent {
 public:
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
   * The current clamp only measures AC, so any DC component is an unwanted artifact from the
   * sampling circuit. The AC component is essentially the same as the calculating the Standard-Deviation,
   * which can be done by cumulating 3 values per sample:
   *   1) Number of samples
   *   2) Sum of samples
   *   3) Sum of sample squared
   * https://en.wikipedia.org/wiki/Root_mean_square
   */

  float last_value_ = 0.0f;
  float sample_sum_ = 0.0f;
  float sample_squared_sum_ = 0.0f;
  uint32_t num_samples_ = 0;
  bool is_sampling_ = false;
};

}  // namespace ct_clamp
}  // namespace esphome
