#ifdef USE_LIBRETINY
#include "ota_backend_arduino_libretiny.h"
#include "ota_backend.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

#include <Update.h>

namespace esphome::ota {

ESPHOME_LOG_TAG(TAG, "ota");

std::unique_ptr<ArduinoLibreTinyOTABackend> make_ota_backend() { return make_unique<ArduinoLibreTinyOTABackend>(); }

OTAResponseTypes ArduinoLibreTinyOTABackend::begin(size_t image_size, OTAType ota_type) {
  if (!this->lock_.try_acquire())
    return OTA_RESPONSE_ERROR_UPDATE_BUSY;
  if (ota_type != OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
  // Handle UPDATE_SIZE_UNKNOWN (0) which is used by web server OTA
  // where the exact firmware size is unknown due to multipart encoding
  if (image_size == 0) {
    image_size = UPDATE_SIZE_UNKNOWN;
  }
  bool ret = Update.begin(image_size, U_FLASH);
  if (ret) {
    this->md5_.init();
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
  if (error == UPDATE_ERROR_SIZE)
    return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;

  ESP_LOGE(TAG, "Begin error: %d", error);

  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoLibreTinyOTABackend::set_update_md5(const char *md5) {
  // Compared in end() before the last block is written, since LibreTiny marks
  // the image bootable before it compares its own MD5
  this->md5_set_ = parse_hex(md5, this->expected_md5_, 16);
}

bool ArduinoLibreTinyOTABackend::write_block_() {
  size_t written = Update.write(this->block_, this->block_len_);
  if (written != this->block_len_) {
    ESP_LOGE(TAG, "Write error: %d", Update.getError());
    return false;
  }
  this->block_len_ = 0;
  this->wrote_ = true;
  return true;
}

OTAResponseTypes ArduinoLibreTinyOTABackend::write(uint8_t *data, size_t len) {
  this->md5_.add(data, len);
  while (len != 0) {
    // Flush a full block only once more data follows, keeping the last one
    if (this->block_len_ == BLOCK_SIZE && !this->write_block_())
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    const size_t chunk = std::min(len, BLOCK_SIZE - this->block_len_);
    memcpy(this->block_ + this->block_len_, data, chunk);
    this->block_len_ += chunk;
    data += chunk;
    len -= chunk;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ArduinoLibreTinyOTABackend::end() {
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_bytes(this->expected_md5_)) {
      ESP_LOGE(TAG, "MD5 mismatch");
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }
  if (!this->write_block_())
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  // Use strict validation (false) when MD5 is set, lenient validation (true) when no MD5
  // This matches the behavior of the old web_server OTA implementation
  if (Update.end(!this->md5_set_)) {
    this->lock_.hold_until_reboot();
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
  ESP_LOGE(TAG, "End error: %d", error);

  return OTA_RESPONSE_ERROR_UPDATE_END;
}

void ArduinoLibreTinyOTABackend::abort() {
  if (!this->lock_.is_held())  // refused in begin(), or end() succeeded
    return;
  Update.abort();
  // A dual-slot bootloader can boot a written image LibreTiny never marked
  // (RTL8720C boots the valid image with the higher serial number), so
  // invalidate it. LN882H's lt_ota_switch() ignores revert, and BK72xx has no
  // mark to undo, so single-slot chips rely on the held-back last block.
  if (this->wrote_ && lt_ota_get_type() == OTA_TYPE_DUAL && !lt_ota_switch(/* revert= */ true)) {
    ESP_LOGE(TAG, "Revert failed");
  }
}

}  // namespace esphome::ota
#endif  // USE_LIBRETINY
