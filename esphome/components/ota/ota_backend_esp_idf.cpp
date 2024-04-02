#ifdef USE_ESP_IDF
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>

#if ESP_IDF_VERSION_MAJOR >= 5
#include <spi_flash_mmap.h>
#endif

namespace esphome {
namespace ota {

OTAResponseTypes IDFOTABackend::begin(size_t image_size) {
  this->partition_ = esp_ota_get_next_update_partition(nullptr);
  if (this->partition_ == nullptr) {
    return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  }

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  // The following function takes longer than the 5 seconds timeout of WDT
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdtc;
  wdtc.idle_core_mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  wdtc.idle_core_mask |= (1 << 0);
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  wdtc.idle_core_mask |= (1 << 1);
#endif
  wdtc.timeout_ms = 15000;
  wdtc.trigger_panic = false;
  esp_task_wdt_reconfigure(&wdtc);
#else
  esp_task_wdt_init(15, false);
#endif
#endif

  esp_err_t err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  // Set the WDT back to the configured timeout
#if ESP_IDF_VERSION_MAJOR >= 5
  wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
  esp_task_wdt_reconfigure(&wdtc);
#else
  esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, false);
#endif
#endif

  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    if (err == ESP_ERR_INVALID_SIZE) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  this->md5_.init();
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::set_update_md5(const char *expected_md5) { memcpy(this->expected_bin_md5_, expected_md5, 32); }

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
  this->md5_.add(data, len);
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      return OTA_RESPONSE_ERROR_MAGIC;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::end() {
  this->md5_.calculate();
  if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
    this->abort();
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }
  esp_err_t err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err == ESP_OK) {
    err = esp_ota_set_boot_partition(this->partition_);
    if (err == ESP_OK) {
      return OTA_RESPONSE_OK;
    }
  }
  if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }
  if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void IDFOTABackend::abort() {
  esp_ota_abort(this->update_handle_);
  this->update_handle_ = 0;
}

}  // namespace ota
}  // namespace esphome
#endif
