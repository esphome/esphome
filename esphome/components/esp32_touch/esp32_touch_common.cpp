#ifdef USE_ESP32

#include "esp32_touch.h"
#include "esphome/core/log.h"
#include <cinttypes>

#include "soc/rtc.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::dump_config_base_() {
  const char *lv_s = get_low_voltage_reference_str(this->low_voltage_reference_);
  const char *hv_s = get_high_voltage_reference_str(this->high_voltage_reference_);
  const char *atten_s = get_voltage_attenuation_str(this->voltage_attenuation_);

  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Meas cycle: %.2fms\n"
                "  Sleep cycle: %.2fms\n"
                "  Low Voltage Reference: %s\n"
                "  High Voltage Reference: %s\n"
                "  Voltage Attenuation: %s\n"
                "  Release Timeout: %" PRIu32 "ms\n",
                this->meas_cycle_ / (8000000.0f / 1000.0f), this->sleep_cycle_ / (150000.0f / 1000.0f), lv_s, hv_s,
                atten_s, this->release_timeout_ms_);
}

void ESP32TouchComponent::dump_config_sensors_() {
  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG,
                  "    Pad: T%u\n"
                  "    Threshold: %" PRIu32 "\n"
                  "    Benchmark: %" PRIu32,
                  (unsigned) child->touch_pad_, child->threshold_, child->benchmark_);
  }
}

bool ESP32TouchComponent::create_touch_queue_() {
  // Queue size calculation: children * 4 allows for burst scenarios where ISR
  // fires multiple times before main loop processes.
  size_t queue_size = this->children_.size() * 4;
  if (queue_size < 8)
    queue_size = 8;

#ifdef USE_ESP32_VARIANT_ESP32
  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEventV1));
#else
  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEventV2));
#endif

  if (this->touch_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create touch event queue of size %" PRIu32, (uint32_t) queue_size);
    this->mark_failed();
    return false;
  }
  return true;
}

void ESP32TouchComponent::cleanup_touch_queue_() {
  if (this->touch_queue_) {
    vQueueDelete(this->touch_queue_);
    this->touch_queue_ = nullptr;
  }
}

void ESP32TouchComponent::configure_wakeup_pads_() {
  bool is_wakeup_source = false;

  // Check if any pad is configured for wakeup
  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      is_wakeup_source = true;

#ifdef USE_ESP32_VARIANT_ESP32
      // ESP32 v1: No filter available when using as wake-up source.
      touch_pad_config(child->get_touch_pad(), child->get_wakeup_threshold());
#else
      // ESP32-S2/S3 v2: Set threshold for wakeup
      touch_pad_set_thresh(child->get_touch_pad(), child->get_wakeup_threshold());
#endif
    }
  }

  if (!is_wakeup_source) {
    // If no pad is configured for wakeup, deinitialize touch pad
    touch_pad_deinit();
  }
}

void ESP32TouchComponent::process_setup_mode_logging_(uint32_t now) {
  if (this->setup_mode_ && now - this->setup_mode_last_log_print_ > SETUP_MODE_LOG_INTERVAL_MS) {
    for (auto *child : this->children_) {
#ifdef USE_ESP32_VARIANT_ESP32
      ESP_LOGD(TAG, "Touch Pad '%s' (T%" PRIu32 "): %" PRIu32, child->get_name().c_str(),
               (uint32_t) child->get_touch_pad(), child->value_);
#else
      // Read the value being used for touch detection
      uint32_t value = this->read_touch_value(child->get_touch_pad());
      // Store the value for get_value() access in lambdas
      child->value_ = value;
      ESP_LOGD(TAG, "Touch Pad '%s' (T%d): %d", child->get_name().c_str(), child->get_touch_pad(), value);
#endif
    }
    this->setup_mode_last_log_print_ = now;
  }
}

bool ESP32TouchComponent::should_check_for_releases_(uint32_t now) {
  if (now - this->last_release_check_ < this->release_check_interval_ms_) {
    return false;
  }
  this->last_release_check_ = now;
  return true;
}

void ESP32TouchComponent::publish_initial_state_if_needed_(ESP32TouchBinarySensor *child, uint32_t now) {
  if (!child->initial_state_published_) {
    // Check if enough time has passed since startup
    if (now > this->release_timeout_ms_) {
      child->publish_initial_state(false);
      child->initial_state_published_ = true;
      ESP_LOGV(TAG, "Touch Pad '%s' state: OFF (initial)", child->get_name().c_str());
    }
  }
}

void ESP32TouchComponent::check_and_disable_loop_if_all_released_(size_t pads_off) {
  // Disable the loop to save CPU cycles when all pads are off and not in setup mode.
  if (pads_off == this->children_.size() && !this->setup_mode_) {
    this->disable_loop();
  }
}

void ESP32TouchComponent::calculate_release_timeout_() {
  // Calculate release timeout based on sleep cycle
  // Design note: Hardware limitation - interrupts only fire reliably on touch (not release)
  // We must use timeout-based detection for release events
  // Formula: 3 sleep cycles converted to ms, with MINIMUM_RELEASE_TIME_MS minimum
  // Per ESP-IDF docs: t_sleep = sleep_cycle / SOC_CLK_RC_SLOW_FREQ_APPROX

  uint32_t rtc_freq = rtc_clk_slow_freq_get_hz();

  // Calculate timeout as 3 sleep cycles
  this->release_timeout_ms_ = (this->sleep_cycle_ * 1000 * 3) / rtc_freq;

  if (this->release_timeout_ms_ < MINIMUM_RELEASE_TIME_MS) {
    this->release_timeout_ms_ = MINIMUM_RELEASE_TIME_MS;
  }

  // Check for releases at 1/4 the timeout interval
  // Since hardware doesn't generate reliable release interrupts, we must poll
  // for releases in the main loop. Checking at 1/4 the timeout interval provides
  // a good balance between responsiveness and efficiency.
  this->release_check_interval_ms_ = this->release_timeout_ms_ / 4;
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32
