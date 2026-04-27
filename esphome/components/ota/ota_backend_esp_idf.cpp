#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <spi_flash_mmap.h>

#ifdef USE_OTA_PARTITIONS
#include "esphome/components/watchdog/watchdog.h"
#include <esp_image_format.h>
#include <nvs_flash.h>
#endif

namespace esphome::ota {

static const char *const TAG = "ota.idf";

std::unique_ptr<IDFOTABackend> make_ota_backend() { return make_unique<IDFOTABackend>(); }

#ifdef USE_OTA_PARTITIONS
OTAResponseTypes IDFOTABackend::begin(size_t image_size, ota::OTAType ota_type) {
  this->ota_type_ = ota_type;
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (image_size > ESP_PARTITION_TABLE_MAX_LEN) {
      ESP_LOGE(TAG, "Wrong partition table size");
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memset(this->buf_, 0xFF, sizeof this->buf_);
    this->buf_written_ = 0;
    this->image_size_ = image_size;
    this->md5_.init();
    return OTA_RESPONSE_OK;
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#else
OTAResponseTypes IDFOTABackend::begin(size_t image_size) {
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

void IDFOTABackend::set_update_md5(const char *expected_md5) {
  memcpy(this->expected_bin_md5_, expected_md5, 32);
  this->md5_set_ = true;
}

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
#ifdef USE_OTA_PARTITIONS
  if (this->ota_type_ == ota::OTA_TYPE_UPDATE_PARTITION_TABLE) {
    if (len > OTA_BUFFER_SIZE - this->buf_written_) {
      ESP_LOGE(TAG, "Wrong partition table size");
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
    }
    memcpy(this->buf_ + this->buf_written_, data, len);
    this->buf_written_ += len;
    this->md5_.add(data, len);
    return OTA_RESPONSE_OK;
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
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
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  }
#endif
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

void IDFOTABackend::abort() {
#ifdef USE_OTA_PARTITIONS
  if (this->partition_table_part_ != nullptr) {
    esp_partition_deregister_external(this->partition_table_part_);
    this->partition_table_part_ = nullptr;
  }
  if (this->ota_type_ != ota::OTA_TYPE_UPDATE_APP) {
    return;
  }
#endif
  esp_ota_abort(this->update_handle_);
  this->update_handle_ = 0;
}

#ifdef USE_OTA_PARTITIONS
static inline bool check_overlap(uint32_t a_offset, size_t a_size, uint32_t b_offset, size_t b_size) {
  return (a_offset + a_size > b_offset && b_offset + b_size > a_offset);
}

OTAResponseTypes IDFOTABackend::update_partition_table() {
  int num_partitions;
  if (this->buf_written_ == 0 || this->image_size_ != this->buf_written_) {
    ESP_LOGE(TAG, "Not enough data received");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Get running app partition and used size
  uint32_t running_app_offset;
  size_t running_app_size;
  get_running_app_position(running_app_offset, running_app_size);

  // Get partition table partition
  esp_err_t err = esp_partition_register_external(
      nullptr, ESP_PRIMARY_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE, "PrimaryPrtTable",
      ESP_PARTITION_TYPE_PARTITION_TABLE, ESP_PARTITION_SUBTYPE_PARTITION_TABLE_PRIMARY, &this->partition_table_part_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_register_external failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Verify existing partition table
  const esp_partition_info_t *existing_partition_table = NULL;
  esp_partition_mmap_handle_t partition_table_map;
  err = esp_partition_mmap(this->partition_table_part_, 0, ESP_PARTITION_TABLE_MAX_LEN, ESP_PARTITION_MMAP_DATA,
                           (const void **) &existing_partition_table, &partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_mmap failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  err = esp_partition_table_verify(existing_partition_table, true, &num_partitions);
  esp_partition_munmap(partition_table_map);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (existing partition table) (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Verify new partition table
  const esp_partition_info_t *new_partition_table = (const esp_partition_info_t *) this->buf_;
  // esp_partition_table_verify expects ESP_PARTITION_TABLE_MAX_LEN bytes of data
  err = esp_partition_table_verify(new_partition_table, true, &num_partitions);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_table_verify failed (new partition table) (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  // Check for missing checksum
  // esp_partition_table_verify does not fail in this case and the ESP would not boot after the update
  bool checksum_found = false;
  for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES; i++) {
    if (((const esp_partition_info_t *) &new_partition_table[i])->magic == ESP_PARTITION_MAGIC_MD5) {
      checksum_found = true;
    }
  }
  if (!checksum_found) {
    ESP_LOGE(TAG, "New partition table has no checksum");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  // Check if the required app and otadata partitions exist in the new partition table
  // Check which app slot to boot from in the new partition table
  int app_partitions_found = 0;
  int new_app_part_index = -1;
  int new_app_part_index_with_copy = -1;
  const esp_partition_t *app_copy_target_part = nullptr;
  bool otadata_partition_found = false;
  bool otadata_overlap = false;
  bool nvs_partition_found = false;
  for (int i = 0; i < num_partitions; i++) {  // Iterate over new partition table
    const esp_partition_info_t *new_part = &new_partition_table[i];
    if (new_part->type == ESP_PARTITION_TYPE_APP) {
      // Found an app partition in the new partition table
      app_partitions_found++;
      if (new_part->pos.size >= running_app_size) {
        // Running app can fit inside this partition
        if (new_part->pos.offset == running_app_offset) {
          // This new app partition can be used for the running app without copying because the offsets are the same
          new_app_part_index = i;
        } else if (!check_overlap(running_app_offset, running_app_size, new_part->pos.offset, running_app_size)) {
          // This new app partition can be used for the running app after copying the app into it
          // Check if there is an app partition in the old partition table at the right offset
          // This is for esp_partition_copy and won't be needed after implementing a better copy function in the future
          esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
          while (it != NULL) {
            const esp_partition_t *p = esp_partition_get(it);
            if (p->address == new_part->pos.offset && p->size >= running_app_size) {
              // Found a suitable pair of partitions in the old and new partition table to copy the running app to
              new_app_part_index_with_copy = i;  // The partition index in the new partition table
              app_copy_target_part = p;          // The partition in the old partition table
            }
            it = esp_partition_next(it);
          }
          esp_partition_iterator_release(it);
        }
      }
    } else if (new_part->type == ESP_PARTITION_TYPE_DATA) {
      if (new_part->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA) {
        // Found the otadata partition in the new partition table
        otadata_partition_found = true;
        otadata_overlap = check_overlap(running_app_offset, running_app_size, new_part->pos.offset, new_part->pos.size);
      } else if (new_part->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS && strcmp((char *) new_part->label, "nvs") == 0) {
        // Found the nvs partition in the new partition table
        nvs_partition_found = true;
      }
    }
  }
  if (new_app_part_index == -1 && new_app_part_index_with_copy == -1) {
    ESP_LOGE(TAG, "No compatible app partition found in the new partition table");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (app_partitions_found < 2 || !otadata_partition_found || !nvs_partition_found) {
    ESP_LOGE(TAG, "New partition table is missing the required app, otadata or nvs partitions");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }
  if (otadata_overlap) {
    ESP_LOGE(TAG, "New otadata partition overlaps with running app");
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_VERIFY;
  }

  ESP_LOGW(TAG, "Checks passed, starting partition table update. Don't remove power until it is completed!");

  // Deinitialize NVS to prevent unwanted flash writes
  nvs_flash_deinit();

  // Copy the running app partition to new position if needed
  if (new_app_part_index == -1) {
    const esp_partition_t *running_app_part = nullptr;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
      const esp_partition_t *p = esp_partition_get(it);
      if (p->address == running_app_offset && p->size >= running_app_size) {
        running_app_part = p;
      }
      it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    ESP_LOGD(TAG, "Copying running app from 0x%X to 0x%X (size: 0x%X)", running_app_part->address,
             app_copy_target_part->address, running_app_size);

    watchdog::WatchdogManager watchdog(15000);
    err = esp_partition_copy(app_copy_target_part, 0, running_app_part, 0, running_app_size);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_partition_copy failed (err=0x%X) ", err);
      return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
    }
  }

  // Update the partition table
  err = esp_ota_begin(this->partition_table_part_, ESP_PARTITION_TABLE_MAX_LEN, &this->update_handle_);
  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_begin failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  err = esp_ota_write(this->update_handle_, this->buf_, ESP_PARTITION_TABLE_MAX_LEN);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_write failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  err = esp_ota_end(this->update_handle_);
  this->update_handle_ = 0;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  this->partition_table_part_ = nullptr;
  esp_partition_unload_all();

  // Write otadata to set the new boot partition
  const esp_partition_t *new_boot_partition = nullptr;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it != NULL) {
    const esp_partition_t *p = esp_partition_get(it);
    const esp_partition_info_t *new_part =
        &new_partition_table[new_app_part_index == -1 ? new_app_part_index_with_copy : new_app_part_index];
    if (p->address == new_part->pos.offset) {
      new_boot_partition = p;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  ESP_LOGD(TAG, "Setting next boot partition to 0x%X", new_boot_partition->address);
  err = esp_ota_set_boot_partition(new_boot_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (err=0x%X) ", err);
    return OTA_RESPONSE_ERROR_PARTITION_TABLE_UPDATE;
  }
  return OTA_RESPONSE_OK;
}

void get_running_app_position(uint32_t &offset, size_t &size) {
  // Gets the start address and the used length aligned to sectors of the running app.
  // This function needs to be called once before calling esp_partition_unload_all().
  // The results are stored using static variables because esp_ota_get_running_partition()
  // does not return valid data after calling esp_partition_unload_all().
  static uint32_t running_app_offset = 0;
  static size_t running_app_size = 0;
  if (running_app_size == 0) {
    const esp_partition_t *running_app_part = esp_ota_get_running_partition();
    running_app_size = running_app_part->size;
    running_app_offset = running_app_part->address;
    const esp_partition_pos_t running_app_pos = {
        .offset = running_app_part->address,
        .size = running_app_part->size,
    };
    esp_image_metadata_t image_metadata;
    image_metadata.start_addr = running_app_part->address;
    esp_err_t err = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &running_app_pos, &image_metadata);
    if (err == ESP_OK && image_metadata.image_len < running_app_part->size) {
      running_app_size = image_metadata.image_len;
    }
    // Align running_app_size to flash sectors
    running_app_size = ((running_app_size + running_app_part->erase_size - 1) / running_app_part->erase_size) *
                       running_app_part->erase_size;
  }
  offset = running_app_offset;
  size = running_app_size;
}
#endif

}  // namespace esphome::ota
#endif  // USE_ESP32
