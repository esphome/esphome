#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <esp_idf_version.h>

#include <vector>

#include <driver/touch_sensor.h>

namespace esphome {
namespace esp32_touch {

class ESP32TouchBinarySensor;

class ESP32TouchComponent : public Component {
 public:
  void register_touch_pad(ESP32TouchBinarySensor *pad) { this->children_.push_back(pad); }

  void set_setup_mode(bool setup_mode) { this->setup_mode_ = setup_mode; }
  void set_sleep_duration(uint16_t sleep_duration) { this->sleep_cycle_ = sleep_duration; }
  void set_measurement_duration(uint16_t meas_cycle) { this->meas_cycle_ = meas_cycle; }
  void set_low_voltage_reference(touch_low_volt_t low_voltage_reference) {
    this->low_voltage_reference_ = low_voltage_reference;
  }
  void set_high_voltage_reference(touch_high_volt_t high_voltage_reference) {
    this->high_voltage_reference_ = high_voltage_reference;
  }
  void set_voltage_attenuation(touch_volt_atten_t voltage_attenuation) {
    this->voltage_attenuation_ = voltage_attenuation;
  }
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  void set_filter_mode(touch_filter_mode_t filter_mode) { this->filter_mode_ = filter_mode; }
  void set_debounce_count(uint32_t debounce_count) { this->debounce_count_ = debounce_count; }
  void set_noise_threshold(uint32_t noise_threshold) { this->noise_threshold_ = noise_threshold; }
  void set_jitter_step(uint32_t jitter_step) { this->jitter_step_ = jitter_step; }
  void set_smooth_level(touch_smooth_mode_t smooth_level) { this->smooth_level_ = smooth_level; }
  void set_denoise_grade(touch_pad_denoise_grade_t denoise_grade) { this->grade_ = denoise_grade; }
  void set_denoise_cap(touch_pad_denoise_cap_t cap_level) { this->cap_level_ = cap_level; }
  void set_waterproof_guard_ring_pad(touch_pad_t pad) { this->waterproof_guard_ring_pad_ = pad; }
  void set_waterproof_shield_driver(touch_pad_shield_driver_t drive_capability) {
    this->waterproof_shield_driver_ = drive_capability;
  }
#else
  void set_iir_filter(uint32_t iir_filter) { this->iir_filter_ = iir_filter; }
#endif

  uint32_t component_touch_pad_read(touch_pad_t tp);

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void on_shutdown() override;

 protected:
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  bool filter_configured_() const {
    return (this->filter_mode_ != TOUCH_PAD_FILTER_MAX) && (this->smooth_level_ != TOUCH_PAD_SMOOTH_MAX);
  }
  bool denoise_configured_() const {
    return (this->grade_ != TOUCH_PAD_DENOISE_MAX) && (this->cap_level_ != TOUCH_PAD_DENOISE_CAP_MAX);
  }
  bool waterproof_configured_() const {
    return (this->waterproof_guard_ring_pad_ != TOUCH_PAD_MAX) &&
           (this->waterproof_shield_driver_ != TOUCH_PAD_SHIELD_DRV_MAX);
  }
#else
  bool iir_filter_enabled_() const { return this->iir_filter_ > 0; }
#endif

  std::vector<ESP32TouchBinarySensor *> children_;
  bool setup_mode_{false};
  uint32_t setup_mode_last_log_print_{0};
  // common parameters
  uint16_t sleep_cycle_{4095};
  uint16_t meas_cycle_{65535};
  touch_low_volt_t low_voltage_reference_{TOUCH_LVOLT_0V5};
  touch_high_volt_t high_voltage_reference_{TOUCH_HVOLT_2V7};
  touch_volt_atten_t voltage_attenuation_{TOUCH_HVOLT_ATTEN_0V};
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  touch_filter_mode_t filter_mode_{TOUCH_PAD_FILTER_MAX};
  uint32_t debounce_count_{0};
  uint32_t noise_threshold_{0};
  uint32_t jitter_step_{0};
  touch_smooth_mode_t smooth_level_{TOUCH_PAD_SMOOTH_MAX};
  touch_pad_denoise_grade_t grade_{TOUCH_PAD_DENOISE_MAX};
  touch_pad_denoise_cap_t cap_level_{TOUCH_PAD_DENOISE_CAP_MAX};
  touch_pad_t waterproof_guard_ring_pad_{TOUCH_PAD_MAX};
  touch_pad_shield_driver_t waterproof_shield_driver_{TOUCH_PAD_SHIELD_DRV_MAX};
#else
  uint32_t iir_filter_{0};
#endif
};

/// Simple helper class to expose a touch pad value as a binary sensor.
class ESP32TouchBinarySensor : public binary_sensor::BinarySensor {
 public:
  ESP32TouchBinarySensor(touch_pad_t touch_pad, uint32_t threshold, uint32_t wakeup_threshold);

  touch_pad_t get_touch_pad() const { return this->touch_pad_; }
  uint32_t get_threshold() const { return this->threshold_; }
  void set_threshold(uint32_t threshold) { this->threshold_ = threshold; }
  uint32_t get_value() const { return this->value_; }
  uint32_t get_wakeup_threshold() const { return this->wakeup_threshold_; }

 protected:
  friend ESP32TouchComponent;

  touch_pad_t touch_pad_{TOUCH_PAD_MAX};
  uint32_t threshold_{0};
  uint32_t value_{0};
  const uint32_t wakeup_threshold_{0};
};

}  // namespace esp32_touch
}  // namespace esphome

#endif
