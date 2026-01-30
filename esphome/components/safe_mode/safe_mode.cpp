#include "safe_mode.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
#include <esp_ota_ops.h>
#endif

namespace esphome::safe_mode {

static const char *const TAG = "safe_mode";

void SafeModeComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Safe Mode:\n"
                "  Successful after: %" PRIu32 "s\n"
                "  Invoke after: %u attempts\n"
                "  Duration: %" PRIu32 "s",
                this->safe_mode_boot_is_good_after_ / 1000,  // because milliseconds
                this->safe_mode_num_attempts_,
                this->safe_mode_enable_time_ / 1000);  // because milliseconds
#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
  const char *state_str;
  if (this->ota_state_ == ESP_OTA_IMG_NEW) {
    state_str = "not supported";
  } else if (this->ota_state_ == ESP_OTA_IMG_PENDING_VERIFY) {
    state_str = "supported";
  } else {
    state_str = "support unknown";
  }
  ESP_LOGCONFIG(TAG, "  Bootloader rollback: %s", state_str);
#endif

  if (this->safe_mode_rtc_value_ > 1 && this->safe_mode_rtc_value_ != SafeModeComponent::ENTER_SAFE_MODE_MAGIC) {
    auto remaining_restarts = this->safe_mode_num_attempts_ - this->safe_mode_rtc_value_;
    if (remaining_restarts) {
      ESP_LOGW(TAG, "Last reset too quick; invoke in %" PRIu32 " restarts", remaining_restarts);
    } else {
      ESP_LOGW(TAG, "SAFE MODE IS ACTIVE");
    }
  }

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
  const esp_partition_t *last_invalid = esp_ota_get_last_invalid_partition();
  if (last_invalid != nullptr) {
    ESP_LOGW(TAG,
             "OTA rollback detected! Rolled back from partition '%s'\n"
             "The device reset before the boot was marked successful",
             last_invalid->label);
  }
#endif
}

float SafeModeComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void SafeModeComponent::loop() {
  if (!this->boot_successful_ && (millis() - this->safe_mode_start_time_) > this->safe_mode_boot_is_good_after_) {
    // successful boot, reset counter
    ESP_LOGI(TAG, "Boot seems successful; resetting boot loop counter");
    this->clean_rtc();
    this->boot_successful_ = true;
#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
    // Mark OTA partition as valid to prevent rollback
    esp_ota_mark_app_valid_cancel_rollback();
#endif
    // Disable loop since we no longer need to check
    this->disable_loop();
  }
}

void SafeModeComponent::set_safe_mode_pending(const bool &pending) {
  uint32_t current_rtc = this->read_rtc_();

  if (pending && current_rtc != SafeModeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Device will enter on next boot");
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

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
  // Check partition state to detect if bootloader supports rollback
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_get_state_partition(running, &this->ota_state_);
#endif

  uint32_t rtc_val = this->read_rtc_();
  this->safe_mode_rtc_value_ = rtc_val;

  bool is_manual = rtc_val == SafeModeComponent::ENTER_SAFE_MODE_MAGIC;

  if (is_manual) {
    ESP_LOGI(TAG, "Manual mode");
  } else {
    ESP_LOGCONFIG(TAG, "Unsuccessful boot attempts: %" PRIu32, rtc_val);
  }

  if (rtc_val < num_attempts && !is_manual) {
    // increment counter
    this->write_rtc_(rtc_val + 1);
    return false;
  }

  this->clean_rtc();

  if (!is_manual) {
    ESP_LOGE(TAG, "Boot loop detected");
  }

  this->status_set_error();
  this->set_timeout(enable_time, []() {
    ESP_LOGW(TAG, "Timeout, restarting");
    App.reboot();
  });

  // Delay here to allow power to stabilize before Wi-Fi/Ethernet is initialised
  delay(300);  // NOLINT
  App.setup();

  ESP_LOGW(TAG, "SAFE MODE IS ACTIVE");

#ifdef USE_SAFE_MODE_CALLBACK
  this->safe_mode_callback_.call();
#endif

  return true;
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

void SafeModeComponent::clean_rtc() {
  // Save without sync - preferences will be written at shutdown or by IntervalSyncer.
  // This avoids blocking the loop for 50+ ms on flash write. If the device crashes
  // before sync, the boot wasn't really successful anyway and the counter should
  // remain incremented.
  uint32_t val = 0;
  this->rtc_.save(&val);
}

void SafeModeComponent::on_safe_shutdown() {
  if (this->read_rtc_() != SafeModeComponent::ENTER_SAFE_MODE_MAGIC)
    this->clean_rtc();
}

}  // namespace esphome::safe_mode
