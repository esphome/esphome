#include "deep_sleep_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP8266
#include <Esp.h>
#endif

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

bool global_has_deep_sleep = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void DeepSleepComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Deep Sleep...");
  global_has_deep_sleep = true;

  if (this->run_duration_.has_value())
    this->set_timeout(*this->run_duration_, [this]() { this->begin_sleep(); });
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
#ifdef USE_ESP32
void DeepSleepComponent::set_wakeup_pin_mode(WakeupPinMode wakeup_pin_mode) {
  this->wakeup_pin_mode_ = wakeup_pin_mode;
}
void DeepSleepComponent::set_ext1_wakeup(Ext1Wakeup ext1_wakeup) { this->ext1_wakeup_ = ext1_wakeup; }
void DeepSleepComponent::set_touch_wakeup(bool touch_wakeup) { this->touch_wakeup_ = touch_wakeup; }
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

  App.run_safe_shutdown_hooks();

#ifdef USE_ESP32
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

  esp_deep_sleep_start();
#endif

#ifdef USE_ESP8266
  ESP.deepSleep(*this->sleep_duration_);  // NOLINT(readability-static-accessed-through-instance)
#endif
}
float DeepSleepComponent::get_setup_priority() const { return setup_priority::LATE; }
void DeepSleepComponent::prevent_deep_sleep() { this->prevent_ = true; }

}  // namespace deep_sleep
}  // namespace esphome
