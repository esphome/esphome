#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <unordered_map>

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_touch {

class ESP32TouchBinarySensor;

class ESP32TouchComponent : public Component {
 public:
  void register_touch_pad(ESP32TouchBinarySensor *pad) { children_.push_back(pad); }

  void set_setup_mode(bool setup_mode) { setup_mode_ = setup_mode; }

  void set_iir_filter(uint32_t iir_filter) { iir_filter_ = iir_filter; }

  void set_sleep_duration(uint16_t sleep_duration) { sleep_cycle_ = sleep_duration; }

  void set_measurement_duration(uint16_t meas_cycle) { meas_cycle_ = meas_cycle; }

  void set_low_voltage_reference(touch_low_volt_t low_voltage_reference) {
    low_voltage_reference_ = low_voltage_reference;
  }

  void set_high_voltage_reference(touch_high_volt_t high_voltage_reference) {
    high_voltage_reference_ = high_voltage_reference;
  }

  void set_voltage_attenuation(touch_volt_atten_t voltage_attenuation) { voltage_attenuation_ = voltage_attenuation; }

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void on_shutdown() override;

 protected:
  /// Is the IIR filter enabled?
  bool iir_filter_enabled_() const { return iir_filter_ > 0; }

  uint16_t sleep_cycle_{};
  uint16_t meas_cycle_{65535};
  touch_low_volt_t low_voltage_reference_{};
  touch_high_volt_t high_voltage_reference_{};
  touch_volt_atten_t voltage_attenuation_{};
  std::vector<ESP32TouchBinarySensor *> children_;
  bool setup_mode_{false};
  uint32_t setup_mode_last_log_print_{};
  uint32_t iir_filter_{0};
};

/// Simple helper class to expose a touch pad value as a binary sensor.
class ESP32TouchBinarySensor : public binary_sensor::BinarySensor {
 public:
  ESP32TouchBinarySensor(const std::string &name, touch_pad_t touch_pad, uint16_t threshold);
  ESP32TouchBinarySensor(const std::string &name, touch_pad_t touch_pad, uint16_t offset, uint16_t interval,
                         uint16_t sample_size);

  touch_pad_t get_touch_pad() const { return touch_pad_; }
  uint16_t get_threshold() const { return threshold_; }
  void set_threshold(uint16_t threshold) { threshold_ = threshold; }
  uint16_t get_value() const { return value_; }
  uint16_t get_offset() const { return offset_; }
  uint16_t get_interval() const { return interval_; }
  uint16_t get_sample_size() const { return sample_size_; }
  void add_sample(uint16_t sample) { samples_[sample]++; }
  void clear_samples() { samples_.clear(); }
  uint32_t get_last_run() const { return last_run_; }
  void set_last_run(uint32_t last_run) { last_run_ = last_run; }
  uint16_t get_count() const { return count_; }
  void increment_count() { count_++; }
  void reset_count() { count_ = 0; }

  uint16_t find_most_freq();

 protected:
  friend ESP32TouchComponent;

  touch_pad_t touch_pad_;
  uint16_t threshold_{};
  uint16_t value_;
  uint16_t offset_;
  uint16_t interval_;
  uint16_t sample_size_{};
  uint16_t count_{};
  uint32_t last_run_{};
  std::unordered_map<uint16_t, uint16_t> samples_;
};

}  // namespace esp32_touch
}  // namespace esphome

#endif
