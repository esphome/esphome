#include "safe_mode.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>

namespace esphome {
namespace safe_mode {

static const char *const TAG = "safe_mode";

void SafeModeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Safe Mode:");
  ESP_LOGCONFIG(TAG, "  Boot considered successful after %" PRIu32 " seconds",
                this->safe_mode_boot_is_good_after_ / 1000);  // because milliseconds
  ESP_LOGCONFIG(TAG, "  Invoke after %u boot attempts", this->safe_mode_num_attempts_);
  ESP_LOGCONFIG(TAG, "  Remain in safe mode for %" PRIu32 " seconds",
                this->safe_mode_enable_time_ / 1000);  // because milliseconds

  if (this->safe_mode_rtc_value_ > 1 && this->safe_mode_rtc_value_ != SafeModeComponent::ENTER_SAFE_MODE_MAGIC) {
    auto remaining_restarts = this->safe_mode_num_attempts_ - this->safe_mode_rtc_value_;
    if (remaining_restarts) {
      ESP_LOGW(TAG, "Last reset occurred too quickly; safe mode will be invoked in %" PRIu32 " restarts",
               remaining_restarts);
    } else {
      ESP_LOGW(TAG, "SAFE MODE IS ACTIVE");
    }
  }
}

float SafeModeComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void SafeModeComponent::loop() {
  if (!this->boot_successful_ && (millis() - this->safe_mode_start_time_) > this->safe_mode_boot_is_good_after_) {
    // successful boot, reset counter
    ESP_LOGI(TAG, "Boot seems successful; resetting boot loop counter");
    this->clean_rtc();
    this->boot_successful_ = true;
  }
}

void SafeModeComponent::set_safe_mode_pending(const bool &pending) {
  uint32_t current_rtc = this->read_rtc_();

  if (pending && current_rtc != SafeModeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Device will enter safe mode on next boot");
    this->write_rtc_(SafeModeComponent::ENTER_SAFE_MODE_MAGIC);
  }

  if (!pending && current_rtc == SafeModeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Safe mode pending has been cleared");
    this->clean_rtc();
  }
}

bool SafeModeComponent::get_safe_mode_pending() {
  return this->read_rtc_() == SafeModeComponent::ENTER_SAFE_MODE_MAGIC;
}

bool SafeModeComponent::should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time,
                                               uint32_t boot_is_good_after) {
  this->safe_mode_start_time_ = millis();
  this->safe_mode_enable_time_ = enable_time;
  this->safe_mode_boot_is_good_after_ = boot_is_good_after;
  this->safe_mode_num_attempts_ = num_attempts;
  this->rtc_ = global_preferences->make_preference<uint32_t>(233825507UL, false);
  this->safe_mode_rtc_value_ = this->read_rtc_();

  bool is_manual_safe_mode = this->safe_mode_rtc_value_ == SafeModeComponent::ENTER_SAFE_MODE_MAGIC;

  if (is_manual_safe_mode) {
    ESP_LOGI(TAG, "Safe mode invoked manually");
  } else {
    ESP_LOGCONFIG(TAG, "There have been %" PRIu32 " suspected unsuccessful boot attempts", this->safe_mode_rtc_value_);
  }

  if (this->safe_mode_rtc_value_ >= num_attempts || is_manual_safe_mode) {
    this->clean_rtc();

    if (!is_manual_safe_mode) {
      ESP_LOGE(TAG, "Boot loop detected. Proceeding to safe mode");
    }

    this->status_set_error();
    this->set_timeout(enable_time, []() {
      ESP_LOGW(TAG, "Safe mode enable time has elapsed -- restarting");
      App.reboot();
    });

    // Delay here to allow power to stabilize before Wi-Fi/Ethernet is initialised
    delay(300);  // NOLINT
    App.setup();

    ESP_LOGW(TAG, "SAFE MODE IS ACTIVE");

    this->safe_mode_callback_.call();

    return true;
  } else {
    // increment counter
    this->write_rtc_(this->safe_mode_rtc_value_ + 1);
    return false;
  }
}

void SafeModeComponent::write_rtc_(uint32_t val) {
  this->rtc_.save(&val);
  global_preferences->sync();
}

uint32_t SafeModeComponent::read_rtc_() {
  uint32_t val;
  if (!this->rtc_.load(&val))
    return 0;
  return val;
}

void SafeModeComponent::clean_rtc() { this->write_rtc_(0); }

void SafeModeComponent::on_safe_shutdown() {
  if (this->read_rtc_() != SafeModeComponent::ENTER_SAFE_MODE_MAGIC)
    this->clean_rtc();
}

}  // namespace safe_mode
}  // namespace esphome
