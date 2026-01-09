#ifdef USE_ESP32
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "deep_sleep_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace deep_sleep {

// Deep Sleep feature support matrix for ESP32 variants:
//
// | Variant   | ext0 | ext1 | Touch | GPIO wakeup |
// |-----------|------|------|-------|-------------|
// | ESP32     | ✓    | ✓    | ✓     |             |
// | ESP32-S2  | ✓    | ✓    | ✓     |             |
// | ESP32-S3  | ✓    | ✓    | ✓     |             |
// | ESP32-C2  |      |      |       | ✓           |
// | ESP32-C3  |      |      |       | ✓           |
// | ESP32-C5  |      | (✓)  |       | (✓)         |
// | ESP32-C6  |      | ✓    |       | ✓           |
// | ESP32-C61 |      | ✓    |       | ✓           |
// | ESP32-H2  |      | ✓    |       |             |
//
// Notes:
// - (✓) = Supported by hardware but not yet implemented in ESPHome
// - ext0: Single pin wakeup using RTC GPIO (esp_sleep_enable_ext0_wakeup)
// - ext1: Multiple pin wakeup (esp_sleep_enable_ext1_wakeup)
// - Touch: Touch pad wakeup (esp_sleep_enable_touchpad_wakeup)
// - GPIO wakeup: GPIO wakeup for non-RTC pins (esp_deep_sleep_enable_gpio_wakeup)

static const char *const TAG = "deep_sleep";

optional<uint32_t> DeepSleepComponent::get_run_duration_() const {
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    switch (wakeup_cause) {
      case ESP_SLEEP_WAKEUP_EXT0:
      case ESP_SLEEP_WAKEUP_EXT1:
      case ESP_SLEEP_WAKEUP_GPIO:
        return this->wakeup_cause_to_run_duration_->gpio_cause;
      case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return this->wakeup_cause_to_run_duration_->touch_cause;
      default:
        return this->wakeup_cause_to_run_duration_->default_cause;
    }
  }
  return this->run_duration_;
}

void DeepSleepComponent::set_wakeup_pin_mode(WakeupPinMode wakeup_pin_mode) {
  this->wakeup_pin_mode_ = wakeup_pin_mode;
}

#if !defined(USE_ESP32_VARIANT_ESP32C2) && !defined(USE_ESP32_VARIANT_ESP32C3)
void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }
#endif

#if !defined(USE_ESP32_VARIANT_ESP32C2) && !defined(USE_ESP32_VARIANT_ESP32C3) && \
    !defined(USE_ESP32_VARIANT_ESP32C6) && !defined(USE_ESP32_VARIANT_ESP32C61) && !defined(USE_ESP32_VARIANT_ESP32H2)
void DeepSleepComponent::set_touch_wakeup(bool touch_wakeup) { this->touch_wakeup_ = touch_wakeup; }
#endif

void DeepSleepComponent::set_run_duration(WakeupCauseToRunDuration wakeup_cause_to_run_duration) {
  wakeup_cause_to_run_duration_ = wakeup_cause_to_run_duration;
}

void DeepSleepComponent::dump_config_platform_() {
  if (wakeup_pin_ != nullptr) {
    LOG_PIN("  Wakeup Pin: ", this->wakeup_pin_);
  }
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG,
                  "  Default Wakeup Run Duration: %" PRIu32 " ms\n"
                  "  Touch Wakeup Run Duration: %" PRIu32 " ms\n"
                  "  GPIO Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->default_cause, this->wakeup_cause_to_run_duration_->touch_cause,
                  this->wakeup_cause_to_run_duration_->gpio_cause);
  }
}

bool DeepSleepComponent::prepare_to_sleep_() {
  if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_KEEP_AWAKE && this->wakeup_pin_ != nullptr &&
      this->wakeup_pin_->digital_read()) {
    // Defer deep sleep until inactive
    if (!this->next_enter_deep_sleep_) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Waiting for wakeup pin state change");
    }
    this->next_enter_deep_sleep_ = true;
    return false;
  }
  return true;
}

void DeepSleepComponent::deep_sleep_() {
  // Timer wakeup - all variants support this
  if (this->sleep_duration_.has_value())
    esp_sleep_enable_timer_wakeup(*this->sleep_duration_);

    // Single pin wakeup (ext0) - ESP32, S2, S3 only
#if !defined(USE_ESP32_VARIANT_ESP32C2) && !defined(USE_ESP32_VARIANT_ESP32C3) && \
    !defined(USE_ESP32_VARIANT_ESP32C6) && !defined(USE_ESP32_VARIANT_ESP32H2)
  if (this->wakeup_pin_ != nullptr) {
    const auto gpio_pin = gpio_num_t(this->wakeup_pin_->get_pin());
    if (this->wakeup_pin_->get_flags() & gpio::FLAG_PULLUP) {
      gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
    } else if (this->wakeup_pin_->get_flags() & gpio::FLAG_PULLDOWN) {
      gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLDOWN_ONLY);
    }
    gpio_sleep_set_direction(gpio_pin, GPIO_MODE_INPUT);
    gpio_hold_en(gpio_pin);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    // Some ESP32 variants support holding a single GPIO during deep sleep without this function
    // For those variants, gpio_hold_en() is sufficient to hold the pin state during deep sleep
    gpio_deep_sleep_hold_en();
#endif
    bool level = !this->wakeup_pin_->is_inverted();
    if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_INVERT_WAKEUP && this->wakeup_pin_->digital_read()) {
      level = !level;
    }
    esp_sleep_enable_ext0_wakeup(gpio_pin, level);
  }
#endif

  // GPIO wakeup - C2, C3, C6, C61 only
#if defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6) || \
    defined(USE_ESP32_VARIANT_ESP32C61)
  if (this->wakeup_pin_ != nullptr) {
    const auto gpio_pin = gpio_num_t(this->wakeup_pin_->get_pin());
    if (this->wakeup_pin_->get_flags() & gpio::FLAG_PULLUP) {
      gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
    } else if (this->wakeup_pin_->get_flags() & gpio::FLAG_PULLDOWN) {
      gpio_sleep_set_pull_mode(gpio_pin, GPIO_PULLDOWN_ONLY);
    }
    gpio_sleep_set_direction(gpio_pin, GPIO_MODE_INPUT);
    gpio_hold_en(gpio_pin);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    // Some ESP32 variants support holding a single GPIO during deep sleep without this function
    // For those variants, gpio_hold_en() is sufficient to hold the pin state during deep sleep
    gpio_deep_sleep_hold_en();
#endif
    bool level = !this->wakeup_pin_->is_inverted();
    if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_INVERT_WAKEUP && this->wakeup_pin_->digital_read()) {
      level = !level;
    }
    esp_deep_sleep_enable_gpio_wakeup(1 << this->wakeup_pin_->get_pin(),
                                      static_cast<esp_deepsleep_gpio_wake_up_mode_t>(level));
  }
#endif

  // Multiple pin wakeup (ext1) - All except C2, C3
#if !defined(USE_ESP32_VARIANT_ESP32C2) && !defined(USE_ESP32_VARIANT_ESP32C3)
  if (this->ext1_wakeup_.has_value()) {
    esp_sleep_enable_ext1_wakeup(this->ext1_wakeup_->mask, this->ext1_wakeup_->wakeup_mode);
  }
#endif

  // Touch wakeup - ESP32, S2, S3 only
#if !defined(USE_ESP32_VARIANT_ESP32C2) && !defined(USE_ESP32_VARIANT_ESP32C3) && \
    !defined(USE_ESP32_VARIANT_ESP32C6) && !defined(USE_ESP32_VARIANT_ESP32C61) && !defined(USE_ESP32_VARIANT_ESP32H2)
  if (this->touch_wakeup_.has_value() && *(this->touch_wakeup_)) {
    esp_sleep_enable_touchpad_wakeup();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  }
#endif

  esp_deep_sleep_start();
}

}  // namespace deep_sleep
}  // namespace esphome
#endif  // USE_ESP32
