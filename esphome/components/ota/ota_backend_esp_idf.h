#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF

#include "ota_component.h"
#include <esp_ota_ops.h>

namespace esphome {
namespace ota {

class IDFOTABackend : public OTABackend {
 public:
  esp_ota_handle_t update_handle = 0;

  OTAResponseTypes begin(size_t image_size) override {
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr) {
      return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
    }
    esp_err_t err = esp_ota_begin(update_partition, image_size, &update_handle);
    if (err != ESP_OK) {
      esp_ota_abort(update_handle);
      update_handle = 0;
      if (err == ESP_ERR_INVALID_SIZE) {
        return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
      } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
        return OTA_RESPONSE_ERROR_WRITING_FLASH;
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }
    return OTA_RESPONSE_OK;
  }

  void set_update_md5(const char *md5) override {
    // pass
  }

  OTAResponseTypes write(uint8_t *data, size_t len) override {
    esp_err_t err = esp_ota_write(update_handle, data, len);
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

  OTAResponseTypes end() override {
    esp_err_t err = esp_ota_end(update_handle);
    update_handle = 0;
    if (err != ESP_OK) {
      if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        return OTA_RESPONSE_ERROR_UPDATE_END;
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }
    return OTA_RESPONSE_OK;
  }

  void abort() override { esp_ota_abort(update_handle); }
};

}  // namespace ota
}  // namespace esphome
#endif
