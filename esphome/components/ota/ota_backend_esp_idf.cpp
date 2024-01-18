#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#ifdef USE_ESP32

#include <esp_task_wdt.h>

#include "ota_backend_esp_idf.h"
#include "ota_component.h"
#include <esp_ota_ops.h>
#include <esp_flash_internal.h>
#include "esphome/components/md5/md5.h"

#if ESP_IDF_VERSION_MAJOR >= 5
#include <spi_flash_mmap.h>
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
// private function in partition.c to unload partitions and free space allocated by them
extern "C" void unload_partitions(void);
#endif

namespace esphome {
namespace ota {

static const char *const TAG = "ota";

static uint32_t running_partition_start = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t running_partition_size = 0;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

OTAResponseTypes IDFOTABackend::begin(OTAPartitionType bin_type, size_t &image_size) {
  esp_err_t err = ESP_OK;
  this->is_upload_ = (image_size != 0);
  this->bin_type_ = bin_type;

  // On first usage, query our running area so we never overwrite it
  if (running_partition_start == 0) {
    running_partition_start = esp_ota_get_running_partition()->address;
    running_partition_size = esp_ota_get_running_partition()->size;
  }

  switch (this->bin_type_.type) {
    case OTA_BIN_APP:
      this->partition_ = esp_ota_get_next_update_partition(nullptr);
      if (this->partition_ == nullptr) {
        return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
      }
      break;
#ifdef USE_UNPROTECTED_WRITES
    case OTA_BIN_BOOTLOADER:
      err = esp_partition_register_external(
          esp_flash_default_chip, ESP_BOOTLOADER_OFFSET, ESP_PARTITION_TABLE_OFFSET - ESP_BOOTLOADER_OFFSET,
          "bootloader", (esp_partition_type_t) 0x42, (esp_partition_subtype_t) 0, &this->partition_);
      if (err != ESP_OK || this->partition_ == nullptr) {
        ESP_LOGE(TAG, "Error registering bootloader partition. Error: 0x%x Pointer: %p", err,
                 (void *) this->partition_);
        IDFOTABackend::deregister_partitions_();
        return OTA_RESPONSE_ERROR_ESP32_REGISTERING_PARTITION;
      }
      break;
    case OTA_BIN_PARTITION_TABLE:
      err =
          esp_partition_register_external(esp_flash_default_chip, ESP_PARTITION_TABLE_OFFSET, 0x1000, "part-table",
                                          (esp_partition_type_t) 0x42, (esp_partition_subtype_t) 1, &this->partition_);
      if (err != ESP_OK || this->partition_ == nullptr) {
        ESP_LOGE(TAG, "Error registering partition table partition. Error: 0x%x Pointer: %p", err,
                 (void *) this->partition_);
        IDFOTABackend::deregister_partitions_();
        return OTA_RESPONSE_ERROR_ESP32_REGISTERING_PARTITION;
      }
      break;
    case OTA_BIN_PARTITION: {
      esp_partition_iterator_t iterator = esp_partition_find(
          (esp_partition_type_t) this->bin_type_.part_type, (esp_partition_subtype_t) this->bin_type_.part_subtype,
          strlen(this->bin_type_.part_label) ? this->bin_type_.part_label : nullptr);
      while (iterator) {
        this->partition_ = esp_partition_get(iterator);
        if (this->bin_type_.part_index-- == 0)
          break;
        if (this->partition_ != nullptr)
          iterator = esp_partition_next(iterator);
      }
      esp_partition_iterator_release(iterator);
    }
      if (this->partition_ != nullptr) {
        ESP_LOGV(TAG, "Partition found - type: 0x%02x; subtype: 0x%02x; addr: 0x%06x; size: 0x%06x; label: %s",
                 (unsigned int) this->partition_->type, (unsigned int) this->partition_->subtype,
                 (unsigned int) this->partition_->address, (unsigned int) this->partition_->size,
                 this->partition_->label);
      } else {
        ESP_LOGE(TAG, "Partition not found - type: 0x%02x; subtype: 0x%02x; label: %s", this->bin_type_.part_type,
                 this->bin_type_.part_subtype, this->bin_type_.part_label);
        IDFOTABackend::log_partitions();
        return OTA_RESPONSE_ERROR_PARTITION_NOT_FOUND;
      }
      break;
#endif
    default:
      return OTA_RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  // round up to the next erase_size
  size_t erase_size = this->partition_->erase_size ? this->partition_->erase_size : this->partition_->size;
#else
  size_t erase_size = this->partition_->size;
#endif
  size_t image_erase_size = (1 + ((image_size - 1) / erase_size)) * erase_size;
  if (this->is_upload_ && this->partition_->address + image_erase_size > running_partition_start &&
      this->partition_->address < running_partition_start + running_partition_size) {
    ESP_LOGE(TAG, "Aborting to avoid overriding running partition");
    ESP_LOGE(TAG, "New partition - addr: 0x%06x; size: 0x%06x", (unsigned int) this->partition_->address,
             (unsigned int) image_erase_size);
    ESP_LOGE(TAG, "Running partition - addr: 0x%06x; size: 0x%06x", (unsigned int) running_partition_start,
             (unsigned int) running_partition_size);
    IDFOTABackend::log_partitions();
    return OTA_RESPONSE_ERROR_ABORT_OVERRIDE;
  }

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  // The following function takes longer than the 5 seconds timeout of WDT
#if ESP_IDF_VERSION_MAJOR >= 5
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
#else
  esp_task_wdt_init(15, false);
#endif
#endif

  if (this->is_upload_) {
    // Write operation
    switch (this->bin_type_.type) {
      case OTA_BIN_APP:
        err = esp_ota_begin(this->partition_, image_size, &this->update_handle_);
        break;
#ifdef USE_UNPROTECTED_WRITES
      case OTA_BIN_BOOTLOADER:
      case OTA_BIN_PARTITION_TABLE:
      case OTA_BIN_PARTITION:
        if (image_erase_size > this->partition_->size) {
          err = ESP_ERR_INVALID_SIZE;
        } else {
          err = esp_partition_erase_range(this->partition_, 0, image_erase_size);
        }
        break;
#endif
      default:
        return OTA_RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED;  // This should never happen (checked before)
    }
  } else {
    // Read operation
    image_size = this->partition_->size;
  }

#if CONFIG_ESP_TASK_WDT_TIMEOUT_S < 15
  // Set the WDT back to the configured timeout
#if ESP_IDF_VERSION_MAJOR >= 5
  wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
  esp_task_wdt_reconfigure(&wdtc);
#else
  esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, false);
#endif
#endif

  if (err != ESP_OK) {
    this->abort();
    if (err == ESP_ERR_INVALID_SIZE) {
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  this->md5_.init();
  this->data_transfered_ = 0;
  return OTA_RESPONSE_OK;
}

void IDFOTABackend::set_update_md5(const char *expected_md5) { memcpy(this->expected_bin_md5_, expected_md5, 32); }

OTAResponseTypes IDFOTABackend::write(uint8_t *data, size_t len) {
  esp_err_t err;
  switch (this->bin_type_.type) {
    case OTA_BIN_APP:
      err = esp_ota_write(this->update_handle_, data, len);
      break;
#ifdef USE_UNPROTECTED_WRITES
    case OTA_BIN_BOOTLOADER:
    case OTA_BIN_PARTITION_TABLE:
    case OTA_BIN_PARTITION:
      err = esp_partition_write(this->partition_, this->data_transfered_, data, len);
      break;
#endif
    default:
      return OTA_RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED;  // This should never happen (checked before)
  }
  this->md5_.add(data, len);
  this->data_transfered_ += len;
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

OTAResponseTypes IDFOTABackend::read(uint8_t *data, size_t len) {
  esp_err_t err;
  switch (this->bin_type_.type) {
    case OTA_BIN_APP:
    case OTA_BIN_BOOTLOADER:
    case OTA_BIN_PARTITION_TABLE:
    case OTA_BIN_PARTITION:
      err = esp_partition_read(this->partition_, this->data_transfered_, data, len);
      break;
    default:
      return OTA_RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED;  // This should never happen (checked before)
  }
  this->md5_.add(data, len);
  this->data_transfered_ += len;
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      return OTA_RESPONSE_ERROR_MAGIC;
    } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
      return OTA_RESPONSE_ERROR_READING_FLASH;
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes IDFOTABackend::end() {
  this->md5_.calculate();
  if (!this->md5_.equals_hex(this->expected_bin_md5_)) {
    this->abort();
    return OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }
  this->deregister_partitions_();
  esp_err_t err = ESP_OK;
  if ((err == ESP_OK) && (this->bin_type_.type == OTA_BIN_APP)) {
    if (this->is_upload_) {
      esp_ota_end(this->update_handle_);
      err = esp_ota_set_boot_partition(this->partition_);
    }
    this->update_handle_ = 0;
  }
  if (err == ESP_OK) {
    return OTA_RESPONSE_OK;
  }
  if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }
  if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void IDFOTABackend::abort() {
  if (this->is_upload_) {
    esp_ota_abort(this->update_handle_);
  }
  this->update_handle_ = 0;
  this->deregister_partitions_();
}

void IDFOTABackend::log_partitions() {
  esp_partition_iterator_t iterator = nullptr;
  const esp_partition_t *next_partition = nullptr;
  iterator = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  ESP_LOGI(TAG, "PARTITION TABLE");
  ESP_LOGI(TAG, "===============");
  while (iterator) {
    next_partition = esp_partition_get(iterator);
    if (next_partition != nullptr) {
      ESP_LOGI(TAG, "type: 0x%02x; subtype: 0x%02x; addr: 0x%06x; size: 0x%06x; label: %s\n",
               (unsigned int) next_partition->type, (unsigned int) next_partition->subtype,
               (unsigned int) next_partition->address, (unsigned int) next_partition->size, next_partition->label);
      iterator = esp_partition_next(iterator);
    }
  }
  esp_partition_iterator_release(iterator);
}
void IDFOTABackend::deregister_partitions_() {
  switch (this->bin_type_.type) {
#ifdef USE_UNPROTECTED_WRITES
    case OTA_BIN_BOOTLOADER:
    case OTA_BIN_PARTITION_TABLE: {
      esp_err_t err = esp_partition_deregister_external(this->partition_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error deregistering partition. Error: 0x%x Pointer: 0x%06X", err,
                 (unsigned int) this->partition_);
      }
      this->partition_ = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
      // Need to undload so they can be refreshed in memory without reboot
      unload_partitions();
#endif
      IDFOTABackend::log_partitions();
      break;
    }
#endif
    default:
      break;
  }
}

}  // namespace ota
}  // namespace esphome
#endif
