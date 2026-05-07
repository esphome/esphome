#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <spi_flash_mmap.h>

namespace esphome::ota {

static const char *const TAG = "ota.idf";

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
    if (image_size > ESP_BOOTLOADER_SIZE) {
      ESP_LOGE(TAG, "Length of received data exceeds the available bootloader size: expected <=%zu bytes, got %zu",
               ESP_BOOTLOADER_SIZE, image_size);
      return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
    }
    OTAResponseTypes result = this->register_and_validate_bootloader_part_();
    if (result != OTA_RESPONSE_OK) {
      return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
    }
    // Validate the partition table to make sure it is at the expected offset, that means the bootloader size is correct
    result = this->register_and_validate_partition_table_part_();
    if (result != OTA_RESPONSE_OK) {
      return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
    }
    esp_partition_deregister_external(this->partition_table_part_);
    this->partition_table_part_ = nullptr;
    ESP_LOGE(TAG, "Starting bootloader update.\n"
                  "  DO NOT REMOVE POWER until the device reboots successfully.\n"
                  "  Loss of power during this operation may render the device\n"
                  "  unable to boot until it is recovered via a serial flash.");
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP && this->ota_type_ != ota::OTA_TYPE_UPDATE_BOOTLOADER) {
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

  watchdog::WatchdogManager watchdog(15000);
  esp_err_t err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin failed (err=0x%X)", err);
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    if (err == ESP_ERR_INVALID_SIZE) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    err = esp_ota_set_final_partition(this->update_handle_, this->bootloader_part_, true);
    if (err != ESP_OK) {
      esp_ota_abort(this->update_handle_);
      this->update_handle_ = 0;
      ESP_LOGE(TAG, "esp_ota_set_final_partition failed (err=0x%X)", err);
      return OTA_RESPONSE_ERROR_BOOTLOADER_UPDATE;
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
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP && this->ota_type_ != ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  esp_err_t err = esp_ota_write(this->update_handle_, data, len);
  this->md5_.add(data, len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X)", err);
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
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    return this->update_partition_table();
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP && this->ota_type_ != ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
  esp_err_t err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err == ESP_OK) {
#ifdef USE_OTA_PARTITIONS
    if (this->ota_type_ == ota::OTA_TYPE_UPDATE_APP)
#endif
    {
      err = esp_ota_set_boot_partition(this->partition_);
      if (err == ESP_OK) {
        return OTA_RESPONSE_OK;
      }
    }
  } else {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X)", err);
  }
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_BOOTLOADER) {
    // TODO: Wipe first sector of this->partition_
    if (err == ESP_OK) {
      esp_partition_deregister_external(this->bootloader_part_);
      this->bootloader_part_ = nullptr;
      return OTA_RESPONSE_OK;
    }
  }
#endif
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
}

}  // namespace esphome::ota
#endif  // USE_ESP32
