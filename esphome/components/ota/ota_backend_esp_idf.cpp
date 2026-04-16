#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <spi_flash_mmap.h>

#ifdef USE_OTA_PARTITIONS
#include <esp_image_format.h>
#endif

namespace esphome::ota {

static const char *const TAG = "ota.idf";

std::unique_ptr<IDFOTABackend> make_ota_backend() { return make_unique<IDFOTABackend>(); }

OTAResponseTypes IDFOTABackend::begin(size_t image_size, ota::OTAType ota_type) {
  this->ota_type_ = ota_type;
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_APP) {
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

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    // The following function takes longer than the 5 seconds timeout of WDT
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
#endif

    esp_err_t err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
    // Set the WDT back to the configured timeout
    wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_reconfigure(&wdtc);
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
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (image_size > ESP_PARTITION_TABLE_SIZE || image_size > ESP_PARTITION_TABLE_MAX_LEN ||
        image_size > OTA_BUFFER_SIZE) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    }
    memset(this->buf_, 0xFF, sizeof this->buf_);
    this->buf_written_ = 0;
    this->image_size_ = image_size;
    this->md5_.init();
    return OTA_RESPONSE_OK;
  }
#endif
  return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
}

void IDFOTABackend::set_update_md5(const char *expected_md5) {
  memcpy(this->expected_bin_md5_, expected_md5, 32);
  this->md5_set_ = true;
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_APP) {
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
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (len > OTA_BUFFER_SIZE - this->buf_written_) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    }
    memcpy(this->buf_ + this->buf_written_, data, len);
    this->buf_written_ += len;
    this->md5_.add(data, len);
    return OTA_RESPONSE_OK;
  }
#endif
  return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
}

OTAResponseTypes IDFOTABackend::end() {
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_APP) {
    esp_err_t err = esp_ota_end(this->update_handle_);
    this->update_handle_ = 0;
    if (err == ESP_OK) {
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
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    return this->update_partition_table();
  }
#endif
  return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
}

void IDFOTABackend::abort() {
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_APP) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
  }
#ifdef USE_OTA_PARTITIONS
  if (this->partition_table_part_ != nullptr) {
    esp_partition_deregister_external(this->partition_table_part_);
    this->partition_table_part_ = nullptr;
  }
#endif
}

#ifdef USE_OTA_PARTITIONS
OTAResponseTypes IDFOTABackend::update_partition_table() {
  esp_err_t err;
  int num_partitions;
  if (this->buf_written_ == 0 || this->image_size_ != this->buf_written_) {
    ESP_LOGE(TAG, "not enough data received (%d/%d bytes)", this->buf_written_, this->image_size_);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  ESP_LOGD(TAG, "partition table size %d", this->image_size_);

  // Get running app partition and used size
  const esp_partition_t *running_app_part = esp_ota_get_running_partition();
  size_t running_app_size = running_app_part->size;
  const esp_partition_pos_t running_app_pos = {
      .offset = running_app_part->address,
      .size = running_app_part->size,
  };
  esp_image_metadata_t image_metadata;
  image_metadata.start_addr = running_app_part->address;
  err = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &running_app_pos, &image_metadata);
  if (err == ESP_OK && image_metadata.image_len < running_app_part->size) {
    running_app_size = image_metadata.image_len;
  }

  // Get partition table partition
  err = esp_partition_register_external(nullptr, ESP_PRIMARY_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE,
                                        "PrimaryPrtTable", ESP_PARTITION_TYPE_PARTITION_TABLE,
                                        ESP_PARTITION_SUBTYPE_PARTITION_TABLE_PRIMARY, &this->partition_table_part_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_register_external failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  // Verify existing partition table
  const esp_partition_info_t *existing_partition_table = NULL;
  esp_partition_mmap_handle_t partition_table_map;
  err = esp_partition_mmap(this->partition_table_part_, 0, ESP_PARTITION_TABLE_MAX_LEN, ESP_PARTITION_MMAP_DATA,
                           (const void **) &existing_partition_table, &partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_mmap failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  err = esp_partition_table_verify(existing_partition_table, true, &num_partitions);
  esp_partition_munmap(partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (existing partition table) (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // Verify new partition table
  const esp_partition_info_t *new_partition_table = (const esp_partition_info_t *) this->buf_;
  // esp_partition_table_verify expects ESP_PARTITION_TABLE_MAX_LEN bytes of data
  err = esp_partition_table_verify(new_partition_table, true, &num_partitions);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (new partition table) (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // Check if the required app and otadata partitions exist in the new partition table
  // Check which app slot to boot from in the new partition table
  int app_partitions_found = 0;
  int app_index = -1;
  int app_index_with_copy = -1;
  int otadata_index = -1;
  bool otadata_no_overlap = false;
  for (int i = 0; i < num_partitions; i++) {
    const esp_partition_info_t *part = &new_partition_table[i];
    if (part->type == ESP_PARTITION_TYPE_APP) {
      app_partitions_found++;
      if (part->pos.size >= running_app_size) {
        if (part->pos.offset == running_app_part->address) {
          app_index = i;
        } else if (part->pos.offset >= running_app_part->address + running_app_size ||
                   running_app_part->address >= part->pos.offset + part->pos.size) {
          // No overlap with running app
          app_index_with_copy = i;
        }
      }
    } else if (part->type == ESP_PARTITION_TYPE_DATA && part->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA) {
      otadata_index = i;
      otadata_no_overlap = part->pos.offset >= running_app_part->address + running_app_size ||
                           running_app_part->address >= part->pos.offset + part->pos.size;
    }
  }
  if (app_index == -1 && app_index_with_copy == -1) {
    // Can't move running app to new partition layout
    ESP_LOGE(TAG, "No compatible app partition found in the new partition table");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (app_partitions_found < 2 || otadata_index == -1) {
    // OTA would be impossible with new partition table
    ESP_LOGE(TAG, "New partition table is missing the required partitions for OTA");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (!otadata_no_overlap) {
    // Can't write to new otadata partition because it overlaps with the running app
    ESP_LOGE(TAG, "New otadata partition overlaps with running app");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  ESP_LOGD(TAG, "Checks passed, starting partition table update", err);

  // TODO: Copy the running app partition to new position if needed
  if (app_index == -1) {
    ESP_LOGE(TAG, "Moving the app partition is required but not implemented");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // Update the partition table
  err = esp_ota_begin(this->partition_table_part_, this->image_size_, &this->update_handle_);
  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_begin failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  err = esp_ota_write(this->update_handle_, this->buf_, this->image_size_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // TODO: Reload partition table and rewrite otadata

  return OTA_RESPONSE_OK;
}
#endif

}  // namespace esphome::ota
#endif  // USE_ESP32
