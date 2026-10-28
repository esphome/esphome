#if defined(USE_ZEPHYR) && !defined(USE_NRF52) && defined(USE_OTA_MCUBOOT)
#include "ota_backend_zephyr.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdint>
#include <cstring>

#include <zephyr/dfu/mcuboot.h>

namespace esphome::ota {

namespace {

ESPHOME_LOG_TAG(TAG, "ota.zephyr");

}  // namespace

std::unique_ptr<ZephyrOTABackend> make_ota_backend() { return make_unique<ZephyrOTABackend>(); }

void ZephyrOTABackend::set_update_md5(const char *md5) {
  if (parse_hex(md5, this->expected_md5_, 16))
    this->md5_set_ = true;
}

// flash_img_init() targets the secondary (slot1) partition, which is the
// standard MCUboot upload target on every dual-slot Zephyr board -- no
// per-chip area id needed here, same as esp_idf's esp_ota_get_next_update_partition().
OTAResponseTypes ZephyrOTABackend::begin(size_t image_size, OTAType ota_type) {
  if (!this->lock_.try_acquire())
    return OTA_RESPONSE_ERROR_UPDATE_BUSY;
  if (ota_type != OTA_TYPE_UPDATE_APP)
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;

  int err = flash_img_init(&this->img_ctx_);
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_init failed: %d", err);
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }

  this->expected_size_ = image_size;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
  this->active_ = true;
  this->md5_.init();

  ESP_LOGD(TAG, "OTA begin: size=%zu", image_size);
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::write(const uint8_t *data, size_t len) {
  if (!this->active_)
    return OTA_RESPONSE_ERROR_WRITING_FLASH;

  int err = flash_img_buffered_write(&this->img_ctx_, data, len, false);
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_buffered_write failed: %d", err);
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  this->md5_.add(data, len);
  this->bytes_written_ += len;
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::end() {
  if (!this->active_)
    return OTA_RESPONSE_ERROR_UPDATE_END;

  int err = flash_img_buffered_write(&this->img_ctx_, nullptr, 0, true);
  this->active_ = false;
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_buffered_write flush failed: %d", err);
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }

  if (this->expected_size_ != 0 && this->bytes_written_ != this->expected_size_) {
    ESP_LOGE(TAG, "Size mismatch: got %zu, expected %zu", this->bytes_written_, this->expected_size_);
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  // A hash that failed to parse must fail the update, not skip the check.
  if (!this->md5_set_) {
    ESP_LOGE(TAG, "No valid MD5 received");
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }
  this->md5_.calculate();
  if (!this->md5_.equals_bytes(this->expected_md5_)) {
    ESP_LOGE(TAG, "MD5 mismatch");
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }

  // Mark the secondary slot pending as a test boot: MCUboot will swap it in
  // on the next reset, and it must be explicitly confirmed after a verified-good
  // boot (safe_mode does this under USE_OTA_ROLLBACK) or it reverts automatically.
  err = boot_request_upgrade(BOOT_UPGRADE_TEST);
  if (err != 0) {
    ESP_LOGE(TAG, "boot_request_upgrade failed: %d", err);
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  ESP_LOGI(TAG, "OTA staged; will boot new image on reboot");
  this->lock_.hold_until_reboot();
  return OTA_RESPONSE_OK;
}

void ZephyrOTABackend::abort() {
  if (!this->lock_.is_held())  // refused in begin()
    return;
  // No explicit cancel API: as long as boot_request_upgrade() was never reached, MCUboot
  // never sees a pending-swap trailer, so a partially-written image is just ignored on
  // next boot -- same trust model as esp_idf's esp_ota_abort().
  this->active_ = false;
  this->expected_size_ = 0;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
}

}  // namespace esphome::ota
#endif  // USE_ZEPHYR && !USE_NRF52
