#ifdef USE_ESP_IDF
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "web_server_idf_ota.h"

esphome::web_server_idf::OTA Update;  // NOLINT

namespace esphome {
namespace web_server_idf {

static const char *const TAG = "web_server_idf_ota";

bool OTA::begin(int image_size) {
  if (this->is_running()) {
    this->abort();
  }
  this->partition_ = esp_ota_get_next_update_partition(nullptr);
  if (this->partition_ == nullptr) {
    this->error_ = ESP_FAIL;
    return false;
  }
  ESP_LOGD(TAG, "Begin with image size %u at partition address 0x%X", image_size, this->partition_->address);
  this->error_ = esp_ota_begin(this->partition_, image_size, &this->handle_);
  if (this->error_ == ESP_OK) {
    return true;
  }
  this->abort();
  return false;
}

void OTA::abort() {
  esp_ota_abort(this->handle_);
  this->handle_ = {};
}

bool OTA::write(const uint8_t *data, int size) {
  if (!this->handle_) {
    if (this->error_ == ESP_OK) {
      this->error_ = ESP_FAIL;
    }
    return false;
  }
  ESP_LOGD(TAG, "Write %u bytes", size);
  this->error_ = esp_ota_write(this->handle_, data, size);
  return this->error_ == ESP_OK;
}

bool OTA::end(bool unused) {
  if (!this->handle_) {
    if (this->error_ == ESP_OK) {
      this->error_ = ESP_FAIL;
    }
    return false;
  }
  ESP_LOGD(TAG, "Finished");
  this->error_ = esp_ota_end(this->handle_);
  this->handle_ = {};
  if (this->error_ == ESP_OK) {
    this->error_ = esp_ota_set_boot_partition(this->partition_);
  }
  return this->error_ == ESP_OK;
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // !defined(USE_ESP_IDF)
