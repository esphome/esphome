#include "deep_sleep_component.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <Esp.h>
#endif

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

bool global_has_deep_sleep = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

optional<uint32_t> DeepSleepComponent::get_run_duration_() const {
#ifdef USE_ESP32
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
#endif
  return this->run_duration_;
}

void DeepSleepComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  global_has_deep_sleep = true;

  const optional<uint32_t> run_duration = get_run_duration_();
  if (run_duration.has_value()) {
    ESP_LOGI(TAG, "Scheduling Deep Sleep to start in %u ms", *run_duration);
    this->set_timeout(*run_duration, [this]() { this->begin_sleep(); });
  } else {
    ESP_LOGD(TAG, "Not scheduling Deep Sleep, as no run duration is configured.");
  }
}
void DeepSleepComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  if (this->sleep_duration_.has_value()) {
    uint32_t duration = *this->sleep_duration_ / 1000;
    ESP_LOGCONFIG(TAG, "  Sleep Duration: %u ms", duration);
  }
  if (this->run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Run Duration: %u ms", *this->run_duration_);
  }
#ifdef USE_ESP32
  if (wakeup_pin_ != nullptr) {
    LOG_PIN("  Wakeup Pin: ", this->wakeup_pin_);
  }
  if (this->wakeup_cause_to_run_duration_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Default Wakeup Run Duration: %u ms", this->wakeup_cause_to_run_duration_->default_cause);
    ESP_LOGCONFIG(TAG, "  Touch Wakeup Run Duration: %u ms", this->wakeup_cause_to_run_duration_->touch_cause);
    ESP_LOGCONFIG(TAG, "  GPIO Wakeup Run Duration: %u ms", this->wakeup_cause_to_run_duration_->gpio_cause);
  }
#endif
}
void DeepSleepComponent::loop() {
  if (this->next_enter_deep_sleep_)
    this->begin_sleep();
}
float DeepSleepComponent::get_loop_priority() const {
  return -100.0f;  // run after everything else is ready
}
void DeepSleepComponent::set_sleep_duration(uint32_t time_ms) { this->sleep_duration_ = uint64_t(time_ms) * 1000; }
#if defined(USE_ESP32)
void DeepSleepComponent::set_wakeup_pin_mode(WakeupPinMode wakeup_pin_mode) {
  this->wakeup_pin_mode_ = wakeup_pin_mode;
}
#endif

#if defined(USE_ESP32)
#if !defined(USE_ESP32_VARIANT_ESP32C3)

void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }

void DeepSleepComponent::set_touch_wakeup(bool touch_wakeup) { this->touch_wakeup_ = touch_wakeup; }

#endif

void DeepSleepComponent::set_run_duration(WakeupCauseToRunDuration wakeup_cause_to_run_duration) {
  wakeup_cause_to_run_duration_ = wakeup_cause_to_run_duration;
}

#endif

void DeepSleepComponent::set_run_duration(uint32_t time_ms) { this->run_duration_ = time_ms; }
void DeepSleepComponent::begin_sleep(bool manual) {
  if (this->prevent_ && !manual) {
    this->next_enter_deep_sleep_ = true;
    return;
  }
#ifdef USE_ESP32
  if (this->wakeup_pin_mode_ == WAKEUP_PIN_MODE_KEEP_AWAKE && this->wakeup_pin_ != nullptr &&
      !this->sleep_duration_.has_value() && this->wakeup_pin_->digital_read()) {
    // Defer deep sleep until inactive
    if (!this->next_enter_deep_sleep_) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Waiting for pin_ to switch state to enter deep sleep...");
    }
    this->next_enter_deep_sleep_ = true;
    return;
  }
#endif

  ESP_LOGI(TAG, "Beginning Deep Sleep");
  if (this->sleep_duration_.has_value()) {
    ESP_LOGI(TAG, "Sleeping for %" PRId64 "us", *this->sleep_duration_);
  }
  App.run_safe_shutdown_hooks();

#if defined(USE_ESP32)
#if !defined(USE_ESP32_VARIANT_ESP32C3)
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
#ifdef USE_ESP32_VARIANT_ESP32C3
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
#endif

#ifdef USE_ESP8266
  ESP.deepSleep(*this->sleep_duration_);  // NOLINT(readability-static-accessed-through-instance)
#endif
}
float DeepSleepComponent::get_setup_priority() const { return setup_priority::LATE; }
void DeepSleepComponent::prevent_deep_sleep() { this->prevent_ = true; }
void DeepSleepComponent::allow_deep_sleep() { this->prevent_ = false; }

}  // namespace deep_sleep
}  // namespace esphome
