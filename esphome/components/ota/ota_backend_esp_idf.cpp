#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF

#include "ota_backend_esp_idf.h"
#include "ota_component.h"
#include <esp_ota_ops.h>
#include "esp32/rom/md5_hash.h"

namespace esphome {
namespace ota {

OTAResponseTypes IDFOTABackend::begin(size_t image_size) {
  this->partition_ = esp_ota_get_next_update_partition(nullptr);
  if (this->partition_ == nullptr) {
    return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  }
  esp_err_t err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);
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
  MD5Init(&this->md5_);
  return OTA_RESPONSE_OK;
}

// TODO: initializing the zeroed bytes when the provided MD5 is of an
// incorrect length is a bit flaky. Check if this can be made a bool
// return value that can be used to send an error back to the OTA client.
void IDFOTABackend::set_update_md5(const char *md5) {
  if (strlen(md5) == 32) {
    char pair[3] = {0, 0, 0};
    for (int i = 0; i < 16; i++) {
      pair[0] = md5[i*2 + 0];
      pair[1] = md5[i*2 + 1];
      this->expected_bin_md5_[i] = (char) strtol(pair, NULL, 16);
    } 
  } else {
    for (int i = 0; i < 16; i++) {
      this->expected_bin_md5_[i] = 0;
    }
  }
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
  MD5Update(&this->md5_, data, len);
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
  if (!this->verify_bin_md5_()) {
    this->abort();
    return OTA_RESPONSE_ERROR_UPDATE_END;
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

bool IDFOTABackend::verify_bin_md5_() {
  unsigned char digest[16];
  MD5Final(digest, &this->md5_);
  for (int i = 0; i < 16; i++) {
    if (digest[i] != this->expected_bin_md5_[i]) {
      return false;
    }
  }
  return true;
}

void IDFOTABackend::abort() {
  esp_ota_abort(this->update_handle_);
  this->update_handle_ = 0;
}

}  // namespace ota
}  // namespace esphome
#endif
