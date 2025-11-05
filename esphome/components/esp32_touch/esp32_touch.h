#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <esp_idf_version.h>

#include <vector>

#include <driver/touch_sensor.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace esp32_touch {

// IMPORTANT: Touch detection logic differs between ESP32 variants:
// - ESP32 v1 (original): Touch detected when value < threshold (capacitance increase causes value decrease)
// - ESP32-S2/S3 v2: Touch detected when value > threshold (capacitance increase causes value increase)
// This inversion is due to different hardware implementations between chip generations.
//
// INTERRUPT BEHAVIOR:
// - ESP32 v1: Interrupts fire when ANY pad is touched and continue while touched.
//   Releases are detected by timeout since hardware doesn't generate release interrupts.
// - ESP32-S2/S3 v2: Hardware supports both touch and release interrupts, but release
//   interrupts are unreliable and sometimes don't fire. We now only use touch interrupts
//   and detect releases via timeout, similar to v1.

static const uint32_t SETUP_MODE_LOG_INTERVAL_MS = 250;

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

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void on_shutdown() override;

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

 protected:
  // Common helper methods
  void dump_config_base_();
  void dump_config_sensors_();
  bool create_touch_queue_();
  void cleanup_touch_queue_();
  void configure_wakeup_pads_();

  // Helper methods for loop() logic
  void process_setup_mode_logging_(uint32_t now);
  bool should_check_for_releases_(uint32_t now);
  void publish_initial_state_if_needed_(ESP32TouchBinarySensor *child, uint32_t now);
  void check_and_disable_loop_if_all_released_(size_t pads_off);
  void calculate_release_timeout_();

  // Common members
  std::vector<ESP32TouchBinarySensor *> children_;
  bool setup_mode_{false};
  uint32_t setup_mode_last_log_print_{0};
  uint32_t last_release_check_{0};
  uint32_t release_timeout_ms_{1500};
  uint32_t release_check_interval_ms_{50};

  // Common configuration parameters
  uint16_t sleep_cycle_{4095};
  uint16_t meas_cycle_{65535};
  touch_low_volt_t low_voltage_reference_{TOUCH_LVOLT_0V5};
  touch_high_volt_t high_voltage_reference_{TOUCH_HVOLT_2V7};
  touch_volt_atten_t voltage_attenuation_{TOUCH_HVOLT_ATTEN_0V};

  // Common constants
  static constexpr uint32_t MINIMUM_RELEASE_TIME_MS = 100;

  // ==================== PLATFORM SPECIFIC ====================

#ifdef USE_ESP32_VARIANT_ESP32
  // ESP32 v1 specific

  static void touch_isr_handler(void *arg);
  QueueHandle_t touch_queue_{nullptr};

 private:
  // Touch event structure for ESP32 v1
  // Contains touch pad info, value, and touch state for queue communication
  struct TouchPadEventV1 {
    touch_pad_t pad;
    uint32_t value;
    bool is_touched;
  };

 protected:
  uint32_t iir_filter_{0};

  bool iir_filter_enabled_() const { return this->iir_filter_ > 0; }

#elif defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  // ESP32-S2/S3 v2 specific
  static void touch_isr_handler(void *arg);
  QueueHandle_t touch_queue_{nullptr};

 private:
  // Touch event structure for ESP32 v2 (S2/S3)
  // Contains touch pad and interrupt mask for queue communication
  struct TouchPadEventV2 {
    touch_pad_t pad;
    uint32_t intr_mask;
  };

 protected:
  // Filter configuration
  touch_filter_mode_t filter_mode_{TOUCH_PAD_FILTER_MAX};
  uint32_t debounce_count_{0};
  uint32_t noise_threshold_{0};
  uint32_t jitter_step_{0};
  touch_smooth_mode_t smooth_level_{TOUCH_PAD_SMOOTH_MAX};

  // Denoise configuration
  touch_pad_denoise_grade_t grade_{TOUCH_PAD_DENOISE_MAX};
  touch_pad_denoise_cap_t cap_level_{TOUCH_PAD_DENOISE_CAP_MAX};

  // Waterproof configuration
  touch_pad_t waterproof_guard_ring_pad_{TOUCH_PAD_MAX};
  touch_pad_shield_driver_t waterproof_shield_driver_{TOUCH_PAD_SHIELD_DRV_MAX};

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

  // Helper method to read touch values - non-blocking operation
  // Returns the current touch pad value using either filtered or raw reading
  // based on the filter configuration
  uint32_t read_touch_value(touch_pad_t pad) const;

  // Helper to update touch state with a known state and value
  void update_touch_state_(ESP32TouchBinarySensor *child, bool is_touched, uint32_t value);

  // Helper to read touch value and update state for a given child
  bool check_and_update_touch_state_(ESP32TouchBinarySensor *child);
#endif

  // Helper functions for dump_config - common to both implementations
  static const char *get_low_voltage_reference_str(touch_low_volt_t ref) {
    switch (ref) {
      case TOUCH_LVOLT_0V5:
        return "0.5V";
      case TOUCH_LVOLT_0V6:
        return "0.6V";
      case TOUCH_LVOLT_0V7:
        return "0.7V";
      case TOUCH_LVOLT_0V8:
        return "0.8V";
      default:
        return "UNKNOWN";
    }
  }

  static const char *get_high_voltage_reference_str(touch_high_volt_t ref) {
    switch (ref) {
      case TOUCH_HVOLT_2V4:
        return "2.4V";
      case TOUCH_HVOLT_2V5:
        return "2.5V";
      case TOUCH_HVOLT_2V6:
        return "2.6V";
      case TOUCH_HVOLT_2V7:
        return "2.7V";
      default:
        return "UNKNOWN";
    }
  }

  static const char *get_voltage_attenuation_str(touch_volt_atten_t atten) {
    switch (atten) {
      case TOUCH_HVOLT_ATTEN_1V5:
        return "1.5V";
      case TOUCH_HVOLT_ATTEN_1V:
        return "1V";
      case TOUCH_HVOLT_ATTEN_0V5:
        return "0.5V";
      case TOUCH_HVOLT_ATTEN_0V:
        return "0V";
      default:
        return "UNKNOWN";
    }
  }
};

/// Simple helper class to expose a touch pad value as a binary sensor.
class ESP32TouchBinarySensor : public binary_sensor::BinarySensor {
 public:
  ESP32TouchBinarySensor(touch_pad_t touch_pad, uint32_t threshold, uint32_t wakeup_threshold)
      : touch_pad_(touch_pad), threshold_(threshold), wakeup_threshold_(wakeup_threshold) {}

  touch_pad_t get_touch_pad() const { return this->touch_pad_; }
  uint32_t get_threshold() const { return this->threshold_; }
  void set_threshold(uint32_t threshold) { this->threshold_ = threshold; }

  /// Get the raw touch measurement value.
  /// @note Although this method may appear unused within the component, it is a public API
  /// used by lambdas in user configurations for custom touch value processing.
  /// @return The current raw touch sensor reading
  uint32_t get_value() const { return this->value_; }

  uint32_t get_wakeup_threshold() const { return this->wakeup_threshold_; }

 protected:
  friend ESP32TouchComponent;

  touch_pad_t touch_pad_{TOUCH_PAD_MAX};
  uint32_t threshold_{0};
  uint32_t benchmark_{};
  /// Stores the last raw touch measurement value.
  uint32_t value_{0};
  bool last_state_{false};
  const uint32_t wakeup_threshold_{0};

  // Track last touch time for timeout-based release detection
  // Design note: last_touch_time_ does not require synchronization primitives because:
  // 1. ESP32 guarantees atomic 32-bit aligned reads/writes
  // 2. ISR only writes timestamps, main loop only reads
  // 3. Timing tolerance allows for occasional stale reads (50ms check interval)
  // 4. Queue operations provide implicit memory barriers
  // Using atomic/critical sections would add overhead without meaningful benefit
  uint32_t last_touch_time_{};
  bool initial_state_published_{};
};

}  // namespace esp32_touch
}  // namespace esphome

#endif
