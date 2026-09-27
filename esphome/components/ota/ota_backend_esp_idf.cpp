#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <esp_ota_ops.h>
#include <sdkconfig.h>
#include <spi_flash_mmap.h>
#ifdef USE_OTA_DOWNGRADE_PROTECTION
#include <esp_app_desc.h>
#endif

namespace esphome::ota {

static const char *const TAG = "ota";

std::unique_ptr<IDFOTABackend> make_ota_backend() { return make_unique<IDFOTABackend>(); }

OTAResponseTypes IDFOTABackend::begin(size_t image_size, ota::OTAType ota_type) {
#ifdef USE_OTA_PARTITIONS
  this->ota_type_ = ota_type;
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    // Reject any size other than ESP_PARTITION_TABLE_MAX_LEN
    if (image_size != ESP_PARTITION_TABLE_MAX_LEN) {
      ESP_LOGE(TAG, "Wrong partition table size: expected %u bytes, got %zu", ESP_PARTITION_TABLE_MAX_LEN, image_size);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memset(this->buf_, 0xFF, sizeof this->buf_);
    this->buf_written_ = 0;
    this->image_size_ = image_size;
    this->md5_.init();
    return OTA_RESPONSE_OK;
  }
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    OTAResponseTypes result = this->prepare_bootloader_update_(image_size);
    if (result != OTA_RESPONSE_OK) {
      return result;
    }
  }
  if (!this->is_app_or_bootloader_update_()) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#else
  if (ota_type != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
#ifdef USE_OTA_ROLLBACK
  // If we're starting an OTA, the current boot is good enough - mark it valid
  // to prevent rollback and allow the OTA to proceed even if the safe mode
  // timer hasn't expired yet.
  esp_ota_mark_app_valid_cancel_rollback();
#endif

  this->partition_ = esp_ota_get_next_update_partition(nullptr);
  if (this->partition_ == nullptr) {
    return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
  }

  // Both lazy-erase paths below replace esp_ota_begin()'s blocking full erase.
  // Size check replaces the one that erase performed (0 = unknown size,
  // e.g. web_server uploads).
  if (image_size != 0 && image_size > this->partition_->size) {
    return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
  }
  this->written_ = 0;
  esp_err_t err;
#ifdef USE_OTA_BLOCK_ERASE_AHEAD
  this->erased_end_ = 0;
  // Unlike esp_ota_begin(), esp_ota_resume() does not reject a running app in
  // ESP_OTA_IMG_PENDING_VERIFY; that state is unreachable here because the app
  // was marked valid at boot (esp32/hal.cpp) or just above under USE_OTA_ROLLBACK.
  // erase_size 0 (!= OTA_WITH_SEQUENTIAL_WRITES) means no erase; erase_ahead_() handles it
  err = esp_ota_resume(this->partition_, 0, 0, &this->update_handle_);
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // esp_ota_begin() does this on IDF 5.5+; esp_ota_resume() does not. Prevents
  // booting a half-written slot after a crash mid-OTA. Not available on the
  // 5.3.3/5.4.2 backports, whose esp_ota_begin() did not invalidate either.
  if (err == ESP_OK) {
    esp_ota_invalidate_inactive_ota_data_slot();
  }
#endif
#else
  err = esp_ota_begin(this->partition_, OTA_WITH_SEQUENTIAL_WRITES, &this->update_handle_);
#endif

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA begin failed (err=0x%X)", err);
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    } else if (err == ESP_ERR_OTA_PARTITION_CONFLICT) {
      // This error appears with 1 factory and 1 ota partition
      return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    OTAResponseTypes result = this->setup_bootloader_staging_();
    if (result != OTA_RESPONSE_OK) {
      return result;
    }
  }
#endif
  this->md5_.init();
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::set_update_md5(const char *expected_md5) {
  memcpy(this->expected_bin_md5_, expected_md5, 32);
  this->md5_set_ = true;
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (len > PARTITION_TABLE_BUFFER_SIZE - this->buf_written_) {
      ESP_LOGE(TAG, "Wrong partition table size");
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memcpy(this->buf_ + this->buf_written_, data, len);
    this->buf_written_ += len;
    this->md5_.add(data, len);
    return OTA_RESPONSE_OK;
  }
  if (!this->is_app_or_bootloader_update_()) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  // Overflow can only happen on unknown-size uploads (web_server); known
  // sizes were rejected in begin().
  if (this->written_ + len > this->partition_->size) {
    return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
  }
#ifdef USE_OTA_BLOCK_ERASE_AHEAD
  OTAResponseTypes erase_result = this->erase_ahead_(len);
  if (erase_result != OTA_RESPONSE_OK) {
    return erase_result;
  }
#endif
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
  this->md5_.add(data, len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X)", err);
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      return OTA_RESPONSE_ERROR_MAGIC;
    } else if (err == ESP_ERR_INVALID_SIZE) {
      // Sequential-writes fallback: IDF's lazy erase reports overflow here
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  this->written_ += len;
  return OTA_RESPONSE_OK;
}

#ifdef USE_OTA_BLOCK_ERASE_AHEAD
OTAResponseTypes IDFOTABackend::erase_ahead_(size_t len) {
  const size_t end = this->written_ + len;
  if (this->erased_end_ >= end) {
    return OTA_RESPONSE_OK;
  }
  // Round up to a block boundary, clamped to the partition end; IDF splits the
  // range into 64 KiB block erases where aligned, sector erases elsewhere.
  const size_t erase_to = next_erase_end(end, this->partition_->size);
  // A block erase is one uninterruptible flash op (typically ~150 ms, seconds
  // on aged flash) and the transfer loop may not have fed the WDT for ~1s.
  watchdog::WatchdogManager watchdog(15000);
  esp_err_t err = esp_partition_erase_range(this->partition_, this->erased_end_, erase_to - this->erased_end_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_erase_range failed (err=0x%X)", err);
    return err == ESP_ERR_INVALID_SIZE ? OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE : OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  this->erased_end_ = erase_to;
  return OTA_RESPONSE_OK;
}
#endif

OTAResponseTypes IDFOTABackend::end() {
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }
#ifdef USE_OTA_PARTITIONS
  // A partition-table update carries an MD5 (checked by IDF), not a Secure Boot
  // signature, and only re-points boot at an already-installed app -- so it is
  // intentionally not run through the signature verifier below.
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    return this->update_partition_table();
  }
  if (!this->is_app_or_bootloader_update_()) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  esp_err_t err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X)", err);
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    return this->finalize_bootloader_update_(err);
  }
#endif
  if (err == ESP_OK) {
#ifdef USE_OTA_SIGNED_VERIFICATION_MULTI_KEY
    // IDF's built-in on-update check is disabled for this scheme (it only
    // matches the incoming image's first signature block against the running
    // app's first). Verify here against every key the running app trusts, so
    // rotation and backup keys are accepted. Leaving the boot partition
    // unchanged means a rejected image never boots.
    if (!this->verify_signed_image_(this->partition_)) {
      return OTA_RESPONSE_ERROR_SIGNATURE_INVALID;
    }
#endif
#ifdef USE_OTA_DOWNGRADE_PROTECTION
    // The image is written and (when signing is enabled) signature-verified by
    // esp_ota_end(), so its embedded project version can be trusted. Reject the
    // update if it is older than the running version by leaving the boot
    // partition unchanged -- the staged image simply never boots.
    esp_app_desc_t incoming;
    esp_err_t desc_err = esp_ota_get_partition_description(this->partition_, &incoming);
    if (desc_err != ESP_OK) {
      // Couldn't read the staged image's version, so the comparison is skipped.
      // Warn so the bypassed check is observable rather than silent.
      ESP_LOGW(TAG, "Downgrade protection: could not read image version (err=0x%X); allowing update", desc_err);
    } else if (version_is_older(incoming.version, ESPHOME_PROJECT_VERSION)) {
      ESP_LOGE(TAG, "Rejecting downgrade: image version '%s' is older than running version '%s'", incoming.version,
               ESPHOME_PROJECT_VERSION);
      return OTA_RESPONSE_ERROR_VERSION_DOWNGRADE;
    }
#endif
    err = esp_ota_set_boot_partition(this->partition_);
    if (err == ESP_OK) {
      return OTA_RESPONSE_OK;
    }
  }
  if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
#ifdef USE_OTA_SIGNED_VERIFICATION
    ESP_LOGE(TAG, "OTA validation failed (err=0x%X) - possible signature verification failure", err);
    return OTA_RESPONSE_ERROR_SIGNATURE_INVALID;
#else
    return OTA_RESPONSE_ERROR_UPDATE_END;
#endif
  }
  if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void IDFOTABackend::abort() {
#ifdef USE_OTA_PARTITIONS
  if (this->partition_table_part_ != nullptr) {
    esp_partition_deregister_external(this->partition_table_part_);
    this->partition_table_part_ = nullptr;
  }
  if (this->bootloader_part_ != nullptr) {
    esp_partition_deregister_external(this->bootloader_part_);
    this->bootloader_part_ = nullptr;
  }
#endif
  // esp_ota_abort with handle 0 returns ESP_ERR_INVALID_ARG harmlessly, so this is safe whether
  // or not an update is in flight.
  esp_ota_abort(this->update_handle_);
  this->update_handle_ = 0;
  this->written_ = 0;
#ifdef USE_OTA_BLOCK_ERASE_AHEAD
  this->erased_end_ = 0;
#endif
}

}  // namespace esphome::ota
#endif  // USE_ESP32
