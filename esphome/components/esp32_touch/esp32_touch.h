#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <vector>

#include <driver/touch_sens.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome::esp32_touch {

// IMPORTANT: Touch detection logic differs between ESP32 variants:
// - ESP32 v1 (original): Touch detected when value < threshold (absolute threshold, capacitance increase causes
//   value decrease)
// - ESP32-S2/S3 v2, ESP32-P4 v3: Touch detected when (smooth - benchmark) > threshold (relative threshold)
//
// CALLBACK BEHAVIOR:
// - ESP32 v1: on_active/on_inactive fire from a software filter timer (esp_timer context).
//   The software filter MUST be configured for these callbacks to fire.
// - ESP32-S2/S3 v2, ESP32-P4 v3: on_active/on_inactive fire from hardware ISR context.
//   Release detection via on_inactive is used, with timeout as safety fallback.

class ESP32TouchBinarySensor;

class ESP32TouchComponent final : public Component {
 public:
  void register_touch_pad(ESP32TouchBinarySensor *pad) { this->children_.push_back(pad); }

  void set_setup_mode(bool setup_mode) { this->setup_mode_ = setup_mode; }
  void set_meas_interval_us(float meas_interval_us) { this->meas_interval_us_ = meas_interval_us; }

#ifdef USE_ESP32_VARIANT_ESP32
  void set_charge_duration_ms(float charge_duration_ms) { this->charge_duration_ms_ = charge_duration_ms; }
#else
  void set_charge_times(uint32_t charge_times) { this->charge_times_ = charge_times; }
#endif

#if !defined(USE_ESP32_VARIANT_ESP32P4)
  void set_low_voltage_reference(touch_volt_lim_l_t low_voltage_reference) {
    this->low_voltage_reference_ = low_voltage_reference;
  }
  void set_high_voltage_reference(touch_volt_lim_h_t high_voltage_reference) {
    this->high_voltage_reference_ = high_voltage_reference;
  }
#endif

  void setup() override;
  void dump_config() override;
  void loop() override;

  void on_shutdown() override;

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
  void set_filter_mode(touch_benchmark_filter_mode_t filter_mode) {
    this->filter_mode_ = filter_mode;
    this->filter_configured_ = true;
  }
  void set_debounce_count(uint32_t debounce_count) {
    this->debounce_count_ = debounce_count;
    this->filter_configured_ = true;
  }
  void set_noise_threshold(uint32_t noise_threshold) {
    this->noise_threshold_ = noise_threshold;
    this->filter_configured_ = true;
  }
  void set_jitter_step(uint32_t jitter_step) {
    this->jitter_step_ = jitter_step;
    this->filter_configured_ = true;
  }
  void set_smooth_level(touch_smooth_filter_mode_t smooth_level) {
    this->smooth_level_ = smooth_level;
    this->filter_configured_ = true;
  }
#if SOC_TOUCH_SUPPORT_DENOISE_CHAN
  void set_denoise_grade(touch_denoise_chan_resolution_t denoise_grade) {
    this->denoise_grade_ = denoise_grade;
    this->denoise_configured_ = true;
  }
  void set_denoise_cap(touch_denoise_chan_cap_t cap_level) {
    this->denoise_cap_level_ = cap_level;
    this->denoise_configured_ = true;
  }
#endif
  void set_waterproof_guard_ring_pad(int channel_id) {
    this->waterproof_guard_ring_pad_ = channel_id;
    this->waterproof_configured_ = true;
  }
  void set_waterproof_shield_driver(uint32_t drive_capability) {
    this->waterproof_shield_driver_ = drive_capability;
    this->waterproof_configured_ = true;
  }
#else
  void set_iir_filter(uint32_t iir_filter) { this->iir_filter_ = iir_filter; }
#endif

 protected:
  // Unified touch event for queue communication
  struct TouchEvent {
    int chan_id;
    bool is_active;
  };

  // Common helper methods
  bool create_touch_queue_();
  void cleanup_touch_queue_();
  void configure_wakeup_pads_();

  // Helper methods for loop() logic
  void process_setup_mode_logging_(uint32_t now);
  void publish_initial_state_if_needed_(ESP32TouchBinarySensor *child, uint32_t now);

  // Unified callbacks for new API
  static bool on_active_cb(touch_sensor_handle_t handle, const touch_active_event_data_t *event, void *ctx);
  static bool on_inactive_cb(touch_sensor_handle_t handle, const touch_inactive_event_data_t *event, void *ctx);

  // Common members
  std::vector<ESP32TouchBinarySensor *> children_;
  bool setup_mode_{false};
  uint32_t setup_mode_last_log_print_{0};

  // Controller handle (new API)
  touch_sensor_handle_t sens_handle_{nullptr};
  QueueHandle_t touch_queue_{nullptr};

  // Common configuration parameters
  float meas_interval_us_{320.0f};

#ifdef USE_ESP32_VARIANT_ESP32
  float charge_duration_ms_{1.0f};
#else
  uint32_t charge_times_{500};
#endif

#if !defined(USE_ESP32_VARIANT_ESP32P4)
  touch_volt_lim_l_t low_voltage_reference_{TOUCH_VOLT_LIM_L_0V5};
  touch_volt_lim_h_t high_voltage_reference_{TOUCH_VOLT_LIM_H_2V7};
#endif

#ifdef USE_ESP32_VARIANT_ESP32
  // ESP32 v1 specific
  uint32_t iir_filter_{0};

  bool iir_filter_enabled_() const { return this->iir_filter_ > 0; }

#elif defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
  // ESP32-S2/S3/P4 v2/v3 specific

  // Filter configuration - use sentinel values to detect "not configured"
  touch_benchmark_filter_mode_t filter_mode_{TOUCH_BM_JITTER_FILTER};
  uint32_t debounce_count_{0};
  uint32_t noise_threshold_{0};
  uint32_t jitter_step_{0};
  touch_smooth_filter_mode_t smooth_level_{TOUCH_SMOOTH_NO_FILTER};
  bool filter_configured_{false};

#if SOC_TOUCH_SUPPORT_DENOISE_CHAN
  // Denoise configuration
  touch_denoise_chan_resolution_t denoise_grade_{TOUCH_DENOISE_CHAN_RESOLUTION_BIT12};
  touch_denoise_chan_cap_t denoise_cap_level_{TOUCH_DENOISE_CHAN_CAP_5PF};
  bool denoise_configured_{false};
#endif

  // Waterproof configuration
  int waterproof_guard_ring_pad_{-1};
  uint32_t waterproof_shield_driver_{0};
  bool waterproof_configured_{false};
#endif
};

/// Simple helper class to expose a touch pad value as a binary sensor.
class ESP32TouchBinarySensor : public binary_sensor::BinarySensor {
 public:
  ESP32TouchBinarySensor(int channel_id, uint32_t threshold, uint32_t wakeup_threshold)
      : channel_id_(channel_id), threshold_(threshold), wakeup_threshold_(wakeup_threshold) {}

  int get_channel_id() const { return this->channel_id_; }
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

  int channel_id_;
  touch_channel_handle_t chan_handle_{nullptr};
  uint32_t threshold_{0};
  uint32_t benchmark_{0};
  /// Stores the last raw touch measurement value.
  uint32_t value_{0};
  bool last_state_{false};
  const uint32_t wakeup_threshold_{0};

  // Track last touch time for timeout-based release detection
  bool initial_state_published_{false};
};

}  // namespace esphome::esp32_touch

#endif
