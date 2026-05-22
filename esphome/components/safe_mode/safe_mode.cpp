#include "safe_mode.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>

#ifdef USE_OTA_ROLLBACK
#ifdef USE_ZEPHYR
#include <zephyr/dfu/mcuboot.h>
#elif defined(USE_ESP32)
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_image_format.h>
#endif
#endif

namespace esphome::safe_mode {

static const char *const TAG = "safe_mode";

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK) && !defined(USE_OTA_PARTITIONS)
// Find a non-running app partition. If verify is true, only returns a partition
// whose image passes verification (expensive: reads flash). Returns nullptr if none found.
static const esp_partition_t *find_alternate_app_partition(bool verify) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *result = nullptr;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    const esp_partition_t *p = esp_partition_get(it);
    if (p->address != running->address) {
      if (!verify) {
        result = p;
        break;
      }
      esp_image_metadata_t data = {};
      const esp_partition_pos_t part_pos = {
          .offset = p->address,
          .size = p->size,
      };
      if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &part_pos, &data) == ESP_OK) {
        result = p;
        break;
      }
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  return result;
}
#endif

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
#ifdef USE_OTA_PARTITIONS
    state_str = "support unknown";
#else
    state_str = "not supported";
#endif
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
             " The device reset before the boot was marked successful",
             last_invalid->label);
    if (esp_reset_reason() == ESP_RST_BROWNOUT) {
      ESP_LOGW(TAG, "Last reset was due to brownout - check your power supply!\n"
                    " See https://esphome.io/guides/faq.html#brownout-detector-was-triggered");
    }
  }
  if (!this->app_ota_possible_) {
    ESP_LOGW(TAG, "OTA updates are impossible.");
#ifdef USE_OTA_PARTITIONS
    ESP_LOGW(TAG, " OTA partition table update or serial flashing is required.");
#else
    if (find_alternate_app_partition(false) != nullptr) {
      ESP_LOGW(TAG, " Activate safe mode to reboot to the recovery partition.");
    } else {
      ESP_LOGE(TAG, " No recovery partition available; serial flashing is required.");
    }
#endif
  }
#endif
}

float SafeModeComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void SafeModeComponent::mark_successful() {
  this->clean_rtc();
  this->boot_successful_ = true;
#if defined(USE_OTA_ROLLBACK)
// Mark OTA partition as valid to prevent rollback
#if defined(USE_ZEPHYR)
  if (!boot_is_img_confirmed()) {
    boot_write_img_confirmed();
  }
#elif defined(USE_ESP32)
  // Mark OTA partition as valid to prevent rollback
  esp_ota_mark_app_valid_cancel_rollback();
#endif
#endif
  // Disable loop since we no longer need to check
  this->disable_loop();
}

void SafeModeComponent::loop() {
  if (!this->boot_successful_ &&
      (App.get_loop_component_start_time() - this->safe_mode_start_time_) > this->safe_mode_boot_is_good_after_) {
    // successful boot, reset counter
    ESP_LOGI(TAG, "Boot seems successful; resetting boot loop counter");
    this->mark_successful();
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
  this->rtc_ = global_preferences->make_preference<uint32_t>(RTC_KEY, false);

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK)
  // Check partition state to detect if bootloader supports rollback
  const esp_partition_t *running_part = esp_ota_get_running_partition();
  esp_ota_get_state_partition(running_part, &this->ota_state_);
  const esp_partition_t *next_part = esp_ota_get_next_update_partition(nullptr);
  this->app_ota_possible_ = (next_part != nullptr && next_part != running_part);
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

#if defined(USE_ESP32) && defined(USE_OTA_ROLLBACK) && !defined(USE_OTA_PARTITIONS)
  // Allow recovery of soft-bricked devices
  // Instead of starting safe_mode, reboot to the other app partition if all conditions are met:
  // - app OTA is impossible (for example because the other app partition has type 'factory')
  // - the other app partition contains a valid app (for example Tasmota safeboot image or ESPHome)
  // - allow_partition_access is not configured making recovery via partition table update impossible
  // Image verification is deferred until here so the cost is only paid when entering safe mode,
  // not on every boot.
  if (!this->app_ota_possible_) {
    const esp_partition_t *rollback_part = find_alternate_app_partition(true);
    if (rollback_part != nullptr) {
      esp_err_t err = esp_ota_set_boot_partition(rollback_part);
      if (err == ESP_OK) {
        ESP_LOGW(TAG, "OTA updates are impossible. Rebooting to recovery app.");
        App.reboot();
      } else {
        ESP_LOGE(TAG, "Failed to set recovery boot partition: %s", esp_err_to_name(err));
      }
    }
  }
#endif

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
