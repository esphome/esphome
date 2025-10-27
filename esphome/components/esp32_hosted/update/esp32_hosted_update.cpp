#include "esp32_hosted_update.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <esp_image_format.h>
#include <esp_app_desc.h>
#include <esp_hosted.h>
#include <esp_task_wdt.h>

extern "C" {
#include <esp_hosted_ota.h>
}

namespace esphome {
namespace esp32_hosted {

static const char *const TAG = "esp32_hosted.update";

constexpr size_t CHUNK_SIZE = 1500;

void Esp32HostedUpdate::setup() {
  this->update_info_.title = "ESP32 Hosted Coprocessor";

  // get coprocessor version
  esp_hosted_coprocessor_fwver_t ver_info;
  if (esp_hosted_get_coprocessor_fwversion(&ver_info) == ESP_OK) {
    this->update_info_.current_version = str_sprintf("%d.%d.%d", ver_info.major1, ver_info.minor1, ver_info.patch1);
  } else {
    this->update_info_.current_version = "unknown";
  }
  ESP_LOGD(TAG, "Coprocessor version: %s", this->update_info_.current_version.c_str());

  // get image version
  const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
  if (this->firmware_size_ >= app_desc_offset + sizeof(esp_app_desc_t)) {
    esp_app_desc_t *app_desc = (esp_app_desc_t *) (this->firmware_data_ + app_desc_offset);
    if (app_desc->magic_word == ESP_APP_DESC_MAGIC_WORD) {
      ESP_LOGD(TAG, "Firmware version: %s", app_desc->version);
      ESP_LOGD(TAG, "Project name: %s", app_desc->project_name);
      ESP_LOGD(TAG, "Build date: %s", app_desc->date);
      ESP_LOGD(TAG, "Build time: %s", app_desc->time);
      ESP_LOGD(TAG, "IDF version: %s", app_desc->idf_ver);
      this->update_info_.latest_version = app_desc->version;
      if (this->update_info_.latest_version != this->update_info_.current_version) {
        this->state_ = update::UPDATE_STATE_AVAILABLE;
      } else {
        this->state_ = update::UPDATE_STATE_NO_UPDATE;
      }
    } else {
      ESP_LOGW(TAG, "Invalid app description magic word: 0x%08x (expected 0x%08x)", app_desc->magic_word,
               ESP_APP_DESC_MAGIC_WORD);
      this->state_ = update::UPDATE_STATE_NO_UPDATE;
    }
  } else {
    ESP_LOGW(TAG, "Firmware too small to contain app description");
    this->state_ = update::UPDATE_STATE_NO_UPDATE;
  }

  // publish state
  this->status_clear_error();
  this->publish_state();
}

void Esp32HostedUpdate::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 Hosted Update:\n"
                "  Current Version: %s\n"
                "  Latest Version: %s\n"
                "  Latest Size: %zu bytes",
                this->update_info_.current_version.c_str(), this->update_info_.latest_version.c_str(),
                this->firmware_size_);
}

void Esp32HostedUpdate::perform(bool force) {
  update::UpdateState prev_state = this->state_;

  if (this->state_ != update::UPDATE_STATE_AVAILABLE && !force) {
    ESP_LOGW(TAG, "Update not available");
    return;
  }

  if (this->firmware_data_ == nullptr || this->firmware_size_ == 0) {
    ESP_LOGE(TAG, "No firmware data available");
    return;
  }

  ESP_LOGI(TAG, "Starting OTA update (%zu bytes)", this->firmware_size_);

  this->state_ = update::UPDATE_STATE_INSTALLING;
  this->update_info_.has_progress = false;
  this->publish_state();

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  // extend watchdog timeout for OTA operation
  esp_task_wdt_config_t wdtc;
  wdtc.idle_core_mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  wdtc.idle_core_mask |= (1 << 0);
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  wdtc.idle_core_mask |= (1 << 1);
#endif
  wdtc.timeout_ms = 30000;
  wdtc.trigger_panic = false;
  esp_task_wdt_reconfigure(&wdtc);
#endif

  esp_err_t err = esp_hosted_slave_ota_begin();  // NOLINT
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(err));
#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_reconfigure(&wdtc);
#endif
    this->state_ = prev_state;
    this->status_set_error("Failed to begin OTA");
    this->publish_state();
    return;
  }

  uint8_t chunk[CHUNK_SIZE];
  const uint8_t *data_ptr = this->firmware_data_;
  size_t remaining = this->firmware_size_;
  while (remaining > 0) {
    size_t chunk_size = std::min(remaining, static_cast<size_t>(CHUNK_SIZE));
    memcpy(chunk, data_ptr, chunk_size);
    err = esp_hosted_slave_ota_write(chunk, chunk_size);  // NOLINT
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
      esp_hosted_slave_ota_end();  // NOLINT
#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
      wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
      esp_task_wdt_reconfigure(&wdtc);
#endif
      this->state_ = prev_state;
      this->status_set_error("Failed to write OTA data");
      this->publish_state();
      return;
    }
    data_ptr += chunk_size;
    remaining -= chunk_size;
    App.feed_wdt();
  }

  err = esp_hosted_slave_ota_end();  // NOLINT
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_reconfigure(&wdtc);
#endif
    this->state_ = prev_state;
    this->status_set_error("Failed to end OTA");
    this->publish_state();
    return;
  }

  // activate new firmware
  err = esp_hosted_slave_ota_activate();  // NOLINT
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to activate OTA: %s", esp_err_to_name(err));
#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_reconfigure(&wdtc);
#endif
    this->state_ = prev_state;
    this->status_set_error("Failed to activate OTA");
    this->publish_state();
    return;
  }

  // restore watchdog and update state
  ESP_LOGI(TAG, "OTA update successful");
#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
  esp_task_wdt_reconfigure(&wdtc);
#endif
  this->state_ = update::UPDATE_STATE_NO_UPDATE;
  this->status_clear_error();
  this->publish_state();

  // schedule a restart to ensure everything is in sync
  this->defer("restart", [this]() {
    ESP_LOGI(TAG, "Restarting in 5 seconds...");
    this->set_timeout(5000, []() { App.safe_reboot(); });
  });
}

}  // namespace esp32_hosted
}  // namespace esphome
