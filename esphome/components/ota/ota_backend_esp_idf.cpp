#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF

#include "ota_backend_esp_idf.h"
#include "ota_component.h"
#include <esp_ota_ops.h>

namespace esphome {
namespace ota {

OTAResponseTypes IDFOTABackend::begin(size_t image_size) {
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
  if (update_partition == nullptr) {
    return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  }
  esp_err_t err = esp_ota_begin(update_partition, image_size, &this->update_handle_);
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
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::set_update_md5(const char *md5) {
  // pass
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
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
  esp_err_t err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      return OTA_RESPONSE_ERROR_UPDATE_END;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::abort() { esp_ota_abort(this->update_handle_); }

}  // namespace ota
}  // namespace esphome
#endif
