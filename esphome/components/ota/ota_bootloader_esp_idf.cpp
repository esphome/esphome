#ifdef USE_ESP32
#include "ota_backend_esp_idf.h"

#include "esphome/core/defines.h"

#ifdef USE_OTA_PARTITIONS
#include "esphome/core/log.h"

#include <esp_image_format.h>
#include <esp_ota_ops.h>

namespace esphome::ota {

static const char *const TAG = "ota.idf";

OTAResponseTypes IDFOTABackend::register_and_validate_bootloader_part_() {
  // Register the bootloader partition
  esp_err_t err = esp_partition_register_external(nullptr, ESP_PRIMARY_BOOTLOADER_OFFSET, ESP_BOOTLOADER_SIZE,
                                                  "PrimaryBTLDR", ESP_PARTITION_TYPE_BOOTLOADER,
                                                  ESP_PARTITION_SUBTYPE_BOOTLOADER_PRIMARY, &this->bootloader_part_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_register_external failed (bootloader) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }

  // Verify existing bootloader to make sure ESP_PRIMARY_BOOTLOADER_OFFSET is correct
  esp_image_metadata_t data = {};
  const esp_partition_pos_t part_pos = {
      .offset = this->bootloader_part_->address,
      .size = this->bootloader_part_->size,
  };
  err = esp_image_verify(ESP_IMAGE_VERIFY, &part_pos, &data);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_image_verify failed (existing bootloader) (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  return OTA_RESPONSE_OK;
}

// Pre-esp_ota_begin: enforce size limit, register/verify the existing bootloader, and validate the
// partition table to confirm the bootloader region is at the expected offset (and therefore the
// expected size). The partition table registration is released here; abort() cleans up the
// bootloader registration if any later step fails.
OTAResponseTypes IDFOTABackend::prepare_bootloader_update_(size_t image_size) {
  if (image_size > ESP_BOOTLOADER_SIZE) {
    ESP_LOGE(TAG, "Length of received data exceeds the available bootloader size: expected <=%zu bytes, got %zu",
             ESP_BOOTLOADER_SIZE, image_size);
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  OTAResponseTypes result = this->register_and_validate_bootloader_part_();
  if (result != OTA_RESPONSE_OK) {
    return result;
  }
  result = this->register_and_validate_partition_table_part_();
  if (result != OTA_RESPONSE_OK) {
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  esp_partition_deregister_external(this->partition_table_part_);
  this->partition_table_part_ = nullptr;
  return OTA_RESPONSE_OK;
}

// Post-esp_ota_begin: verify the staging app partition is large enough, erase it, and redirect the
// final write target to the bootloader partition. esp_ota_set_final_partition is called with
// `restore_old_data=false` because we erased the staging region in advance.
OTAResponseTypes IDFOTABackend::setup_bootloader_staging_() {
  if (this->partition_->size < this->bootloader_part_->size) {
    ESP_LOGE(TAG, "Staging partition too small");
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  // Erase full size of the bootloader partition in the staging partition
  // to avoid copying old data to the bootloader partition later
  esp_err_t err = esp_partition_erase_range(this->partition_, 0, this->bootloader_part_->size);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_partition_erase_range failed (err=0x%X)", err);
    // No critical error, don't return
  }
  err = esp_ota_set_final_partition(this->update_handle_, this->bootloader_part_, false);
  if (err != ESP_OK) {
    esp_ota_abort(this->update_handle_);
    this->update_handle_ = 0;
    ESP_LOGE(TAG, "esp_ota_set_final_partition failed (err=0x%X)", err);
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  return OTA_RESPONSE_OK;
}

// After esp_ota_end: copy the staged image into the bootloader partition. esp_partition_copy is
// the only window in which a power loss can render the device unbootable; everything before this
// point either preserves the existing bootloader or fails harmlessly. After a successful copy the
// first sector of staging is wiped so the device can't accidentally boot from it, and the
// bootloader partition is deregistered.
OTAResponseTypes IDFOTABackend::finalize_bootloader_update_(esp_err_t ota_end_err) {
  if (ota_end_err != ESP_OK) {
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
  esp_bootloader_desc_t bootloader_desc;
  esp_err_t desc_err = esp_ota_get_bootloader_description(this->partition_, &bootloader_desc);
#ifdef USE_ESP32_SRAM1_AS_IRAM
  if (desc_err != ESP_OK) {
    ESP_LOGE(TAG, "New bootloader does not support SRAM1 as IRAM");
    return OTA_RESPONSE_ERROR_BOOTLOADER_VERIFY;
  }
#endif
  ESP_LOGE(TAG, "Starting bootloader update.\n"
                "  DO NOT REMOVE POWER until the update completes successfully.\n"
                "  Loss of power during this operation may render the device\n"
                "  unable to boot until it is recovered via a serial flash.");
  esp_err_t err = esp_partition_copy(this->bootloader_part_, 0, this->partition_, 0, this->bootloader_part_->size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_partition_copy failed (err=0x%X)", err);
    // Only if esp_partition_copy failed there's a chance of the device being unbootable
    return OTA_RESPONSE_ERROR_BOOTLOADER_UPDATE;
  }
  ESP_LOGI(TAG,
           "Successfully installed the new bootloader\n"
           "  ESP-IDF %s",
           (desc_err == ESP_OK) ? bootloader_desc.idf_ver : "version unknown");
  // Wipe first sector of staging partition to make sure the device can't boot from it
  err = esp_partition_erase_range(this->partition_, 0, this->partition_->erase_size);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_partition_erase_range failed (err=0x%X)", err);
    // No critical error, don't return
  }
  esp_partition_deregister_external(this->bootloader_part_);
  this->bootloader_part_ = nullptr;
  return OTA_RESPONSE_OK;
}

}  // namespace esphome::ota

#endif  // USE_OTA_PARTITIONS
#endif  // USE_ESP32
