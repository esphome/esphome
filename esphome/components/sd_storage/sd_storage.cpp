#include "sd_storage.h"

#ifdef USE_SD_STORAGE_SDMMC

#include "esphome/core/log.h"
#include <sys/stat.h>
#include <errno.h>
#include <cstdio>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "diskio_sdmmc.h"
#include "sdmmc_cmd.h"
// SD_OCR_SDHC_CAP lives here; sdmmc_cmd.h only pulls in sd_protocol_types.h, so relying on it
// reaching us through another header is what broke once the include chain changed.
#include "sd_protocol_defs.h"
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esphome/components/storage/storage.h"

namespace esphome::sd_storage {

static const char *const TAG = "sd_storage";

void SdMmc::setup() {
  ESP_LOGD(TAG, "Initializing SD/MMC card");
  ESP_LOGD(TAG, "  CLK pin: %d, CMD pin: %d, DATA0 pin: %d", this->clk_pin_, this->cmd_pin_, this->data0_pin_);
  if (!this->mode_1bit_) {
    ESP_LOGD(TAG, "  DATA1 pin: %d, DATA2 pin: %d, DATA3 pin: %d", this->data1_pin_, this->data2_pin_,
             this->data3_pin_);
  } else {
    ESP_LOGD(TAG, "  Operating in 1-bit mode");
  }
  ESP_LOGD(TAG, "  Mount path: %s, Slot: %d", this->mount_path_, this->slot_);

  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->setup();
  }

  // Register before attempting to mount, not only on success -- get_info() reports is_mounted
  // correctly either way, and this lets the device (and its mount/unmount/list_files actions)
  // show up in the registry even if the initial mount fails, instead of only existing once a
  // card happens to be present. No mark_failed() here: a failed mount is not a broken
  // component, and this lets sd_storage.mount retry later without a reboot.
  if (storage::global_storage_registry != nullptr) {
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal -- do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }
  } else {
    // Same contract as StorageWorker::setup(): the registry (BUS priority) is guaranteed to
    // exist by the time this runs. Without it the device is invisible to resolve_path(), so
    // every storage.* action would report "no storage mounted" with no diagnostic at all.
    ESP_LOGE(TAG, "storage registry unavailable -- device cannot be registered");
    this->mark_failed();
  }

  if (this->cd_pin_ != nullptr) {
    this->cd_pin_->setup();
    // With a CD pin configured, only mount if a card is actually seen at boot -- otherwise wait
    // for loop()'s polling to pick up an insertion later, rather than logging a spurious "Failed
    // to mount" for a socket that's simply empty right now. No mark_failed() either way, same
    // rationale as above.
    if (this->card_present_()) {
      if (this->mount() != storage::StorageError::STORAGE_ERROR_OK) {
        ESP_LOGE(TAG, "Failed to mount SD/MMC card");
      }
    } else {
      ESP_LOGD(TAG, "Waiting for card (CD)");
    }
  } else if (this->mount() != storage::StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "Failed to mount SD/MMC card");
  }
}

void SdMmc::loop() { this->loop_cd_(); }

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->is_mounted_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_);
  LOG_PIN("  CD Pin: ", this->cd_pin_);
  ESP_LOGCONFIG(TAG, "  Data rate: %" PRIu32 " kHz", this->data_rate_khz_);
  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %s", SdStorageBase::card_type_to_string(this->card_type_));
    ESP_LOGCONFIG(TAG, "  Total bytes: %" PRIu64, this->total_bytes_);
    ESP_LOGCONFIG(TAG, "  Used bytes: %" PRIu64, this->used_bytes_);
  }
}

storage::StorageError SdMmc::mount() {
  if (this->is_mounted_)
    return storage::StorageError::STORAGE_ERROR_OK;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = static_cast<int>(this->data_rate_khz_);
  host.flags = this->mode_1bit_ ? SDMMC_HOST_FLAG_1BIT : SDMMC_HOST_FLAG_4BIT;
  host.slot = this->slot_;

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = this->mode_1bit_ ? 1 : 4;

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
#endif
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 16,
      .allocation_unit_size = 256 * 1024,
  };

  esp_err_t ret = ESP_FAIL;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGD(TAG, "Mounting SD card slot %d to '%s' (attempt %d/3)", this->slot_, this->mount_path_, attempt);
    ret = esp_vfs_fat_sdmmc_mount(this->mount_path_, &host, &slot_config, &mount_config, &this->card_);
    if (ret == ESP_OK)
      break;
    ESP_LOGW(TAG, "Mount attempt %d failed: %s", attempt, esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  }

  if (this->card_->is_mmc) {
    this->card_type_ = CardType::CARD_TYPE_MMC;
  } else if (this->card_->is_sdio) {
    this->card_type_ = CardType::CARD_TYPE_SDIO;
  } else {
    this->card_type_ = (this->card_->ocr & SD_OCR_SDHC_CAP) ? CardType::CARD_TYPE_SDHC : CardType::CARD_TYPE_SDSC;
  }
  this->block_size_ = this->card_->csd.sector_size;
  this->is_mounted_ = true;
#ifdef USE_STORAGE_CHANGE_FEED
  // Mount state is part of the roots listing: whoever flipped it (CD pin, hotplug, HTTP,
  // automation), the browser's change poll must see it -- including recovery after an error.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->note_dir_changed("");
#endif

  BYTE pdrv = ff_diskio_get_pdrv_card(this->card_);
  if (pdrv == 0xFF) {
    ESP_LOGE(TAG, "No diskio binding for card (pdrv lookup failed); direct FATFS path operations will fail");
  }
  this->set_fatfs_drive_(pdrv);
  this->update_card_info();

  ESP_LOGI(TAG, "SD/MMC card mounted at %s", this->mount_path_);

  if (storage::global_storage_registry != nullptr) {
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal -- do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }
  } else {
    // Same contract as StorageWorker::setup(): the registry (BUS priority) is guaranteed to
    // exist by the time this runs. Without it the device is invisible to resolve_path(), so
    // every storage.* action would report "no storage mounted" with no diagnostic at all.
    ESP_LOGE(TAG, "storage registry unavailable -- device cannot be registered");
    this->mark_failed();
  }

  this->on_mounted_.call(this->mount_path_);

  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError SdMmc::unmount() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return storage::StorageError::STORAGE_ERROR_OK;

  // Quiesce before the VFS unmount below -- same drain guarantee as unregister_storage()
  // (no in-flight storage_worker data-plane call against this device remains, handles the
  // worker opened are closed), but the device stays registered: registered-but-unmounted
  // is its normal state, so there is nothing to re-register afterwards.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->quiesce_storage(this);

  ESP_LOGD(TAG, "Syncing filesystem before unmount");
  // Closes any handles still open from user/lambda code, while the VFS is still mounted to
  // receive the flush/close calls.
  storage::StorageError flush_err = this->flush_open_handles_();
  if (flush_err == storage::StorageError::STORAGE_ERROR_OK) {
    ESP_LOGD(TAG, "All data flushed");
  } else {
    ESP_LOGW(TAG, "Flush before unmount failed: %s", storage::error_to_string(flush_err));
  }

  esp_err_t unmount_err = esp_vfs_fat_sdcard_unmount(this->mount_path_, this->card_);
  if (unmount_err != ESP_OK) {
    ESP_LOGW(TAG, "esp_vfs_fat_sdcard_unmount failed: %s", esp_err_to_name(unmount_err));
  }
  this->card_ = nullptr;
  this->is_mounted_ = false;
#ifdef USE_STORAGE_CHANGE_FEED
  // Mount state is part of the roots listing: whoever flipped it (CD pin, hotplug, HTTP,
  // automation), the browser's change poll must see it -- including recovery after an error.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->note_dir_changed("");
#endif

  // Report the flush and unmount results so a teardown that failed does not look clean.
  if (flush_err != storage::StorageError::STORAGE_ERROR_OK)
    return flush_err;
  if (unmount_err != ESP_OK)
    return storage::StorageError::STORAGE_ERROR_NOT_READY;

  ESP_LOGI(TAG, "SD/MMC card unmounted safely");
  return storage::StorageError::STORAGE_ERROR_OK;
}

bool SdMmc::update_card_info() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return false;

  this->total_bytes_ = (uint64_t) this->card_->csd.capacity * this->card_->csd.sector_size;

  uint64_t total_bytes = 0, free_bytes = 0;
  if (esp_vfs_fat_info(this->mount_path_, &total_bytes, &free_bytes) == ESP_OK)
    this->used_bytes_ = total_bytes - free_bytes;
  return true;
}

uint64_t SdMmc::get_free_bytes_impl() const {
  if (!this->is_mounted_)
    return 0;

  uint64_t total_bytes = 0, free_bytes = 0;
  if (esp_vfs_fat_info(this->mount_path_, &total_bytes, &free_bytes) != ESP_OK)
    return 0;
  return free_bytes;
}

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SDMMC
