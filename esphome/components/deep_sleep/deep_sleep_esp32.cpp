#ifdef USE_ESP32
#include "deep_sleep_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace deep_sleep {

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

#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6)
void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }

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
    ESP_LOGCONFIG(TAG, "  Default Wakeup Run Duration: %" PRIu32 " ms",
                  this->wakeup_cause_to_run_duration_->default_cause);
    ESP_LOGCONFIG(TAG, "  Touch Wakeup Run Duration: %" PRIu32 " ms", this->wakeup_cause_to_run_duration_->touch_cause);
    ESP_LOGCONFIG(TAG, "  GPIO Wakeup Run Duration: %" PRIu32 " ms", this->wakeup_cause_to_run_duration_->gpio_cause);
  }
}

bool DeepSleepComponent::prepare_to_sleep_() {
  if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_KEEP_AWAKE && this->wakeup_pin_ != nullptr &&
      this->wakeup_pin_->digital_read()) {
    // Defer deep sleep until inactive
    if (!this->next_enter_deep_sleep_) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Waiting wakeup pin state change to enter deep sleep...");
    }
    this->next_enter_deep_sleep_ = true;
    return false;
  }
  return true;
}

void DeepSleepComponent::deep_sleep_() {
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6)
  if (this->sleep_duration_.has_value())
    esp_sleep_enable_timer_wakeup(*this->sleep_duration_);
  if (this->wakeup_pin_ != nullptr) {
    bool level = !this->wakeup_pin_->is_inverted();
    if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_INVERT_WAKEUP && this->wakeup_pin_->digital_read()) {
      level = !level;
    }
    esp_sleep_enable_ext0_wakeup(gpio_num_t(this->wakeup_pin_->get_pin()), level);
  }
  if (this->ext1_wakeup_.has_value()) {
    esp_sleep_enable_ext1_wakeup(this->ext1_wakeup_->mask, this->ext1_wakeup_->wakeup_mode);
  }

  if (this->touch_wakeup_.has_value() && *(this->touch_wakeup_)) {
    esp_sleep_enable_touchpad_wakeup();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  }
#endif
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6)
  if (this->sleep_duration_.has_value())
    esp_sleep_enable_timer_wakeup(*this->sleep_duration_);
  if (this->wakeup_pin_ != nullptr) {
    bool level = !this->wakeup_pin_->is_inverted();
    if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_INVERT_WAKEUP && this->wakeup_pin_->digital_read()) {
      level = !level;
    }
    esp_deep_sleep_enable_gpio_wakeup(1 << this->wakeup_pin_->get_pin(),
                                      static_cast<esp_deepsleep_gpio_wake_up_mode_t>(level));
  }
#endif
  esp_deep_sleep_start();
}

}  // namespace deep_sleep
}  // namespace esphome
#endif
