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
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esphome/components/storage/storage.h"

#if defined(USE_ESP32_VARIANT_ESP32P4)
#define SD_MMC_USE_HIGHSPEED
#endif

namespace esphome::sd_storage {

static const char *const TAG = "sd_storage";

void SdMmc::setup() {
  ESP_LOGI(TAG, "Initializing SD/MMC card");
  ESP_LOGI(TAG, "  CLK pin: %d, CMD pin: %d, DATA0 pin: %d", this->clk_pin_, this->cmd_pin_, this->data0_pin_);
  if (!this->mode_1bit_) {
    ESP_LOGI(TAG, "  DATA1 pin: %d, DATA2 pin: %d, DATA3 pin: %d", this->data1_pin_, this->data2_pin_,
             this->data3_pin_);
  } else {
    ESP_LOGI(TAG, "  Operating in 1-bit mode");
  }
  ESP_LOGI(TAG, "  Mount path: %s, Slot: %d", this->mount_path_, this->slot_);

  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->setup();
  }

  // Register before attempting to mount, not only on success — get_info() reports is_mounted
  // correctly either way, and this lets the device (and its mount/unmount/list_files actions)
  // show up in the registry even if the initial mount fails, instead of only existing once a
  // card happens to be present. No mark_failed() here: a failed mount is not a broken
  // component, and this lets sd_storage.mount retry later without a reboot.
  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }

  if (this->cd_pin_ != nullptr) {
    this->cd_pin_->setup();
    // With a CD pin configured, only mount if a card is actually seen at boot — otherwise wait
    // for loop()'s polling to pick up an insertion later, rather than logging a spurious "Failed
    // to mount" for a socket that's simply empty right now. No mark_failed() either way, same
    // rationale as above.
    if (this->card_present_()) {
      if (this->mount() != storage::StorageError::OK) {
        ESP_LOGE(TAG, "Failed to mount SD/MMC card");
      }
    } else {
      ESP_LOGI(TAG, "Waiting for card (CD)");
    }
  } else if (this->mount() != storage::StorageError::OK) {
    ESP_LOGE(TAG, "Failed to mount SD/MMC card");
  }
}

void SdMmc::loop() { this->loop_cd_(); }

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->is_mounted_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_);
  LOG_PIN("  CD Pin: ", this->cd_pin_);
  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %s", SdStorageBase::card_type_to_string(this->card_type_));
    ESP_LOGCONFIG(TAG, "  Total bytes: %" PRIu64, this->total_bytes_);
    ESP_LOGCONFIG(TAG, "  Used bytes: %" PRIu64, this->used_bytes_);
  }
}

storage::StorageError SdMmc::mount() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
#if defined(SD_MMC_USE_HIGHSPEED)
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#else
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;
#endif
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

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  {
    esp_err_t pre_ret = sdmmc_host_init_slot(host.slot, &slot_config);
    if (pre_ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to init SDMMC slot %d: %s", this->slot_, esp_err_to_name(pre_ret));
      return storage::StorageError::NOT_READY;
    }
  }
#endif

  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 16,
      .allocation_unit_size = 256 * 1024,
  };

  esp_err_t ret = ESP_FAIL;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGI(TAG, "Mounting SD card slot %d to '%s' (attempt %d/3)", this->slot_, this->mount_path_, attempt);
    ret = esp_vfs_fat_sdmmc_mount(this->mount_path_, &host, &slot_config, &mount_config, &this->card_);
    if (ret == ESP_OK)
      break;
    ESP_LOGW(TAG, "Mount attempt %d failed: %s", attempt, esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    return storage::StorageError::NOT_READY;
  }

  if (this->card_->is_mmc) {
    this->card_type_ = CardType::MMC;
  } else if (this->card_->is_sdio) {
    this->card_type_ = CardType::SDIO;
  } else {
    this->card_type_ = (this->card_->ocr & (1 << 30)) ? CardType::SDHC : CardType::SDSC;
  }
  this->block_size_ = this->card_->csd.sector_size;
  this->is_mounted_ = true;
  this->set_fatfs_drive_(ff_diskio_get_pdrv_card(this->card_));
  this->update_card_info();

  ESP_LOGI(TAG, "SD/MMC card mounted at %s", this->mount_path_);

  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }

  this->on_mounted_.call(this->mount_path_);

  return storage::StorageError::OK;
}

storage::StorageError SdMmc::unmount() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return storage::StorageError::OK;

  // Unregister before the VFS unmount below — the registry contract guarantees
  // unregister_storage() doesn't return until any in-flight storage_worker data-plane calls
  // against this device have drained (closing any handles the worker itself opened), so it's
  // safe to tear the filesystem down immediately afterward.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->unregister_storage(this);

  ESP_LOGI(TAG, "Syncing filesystem before unmount");
  // Closes any handles still open from user/lambda code, while the VFS is still mounted to
  // receive the flush/close calls.
  this->flush_open_handles_();
  ESP_LOGI(TAG, "All data flushed");

  esp_vfs_fat_sdcard_unmount(this->mount_path_, this->card_);
  this->card_ = nullptr;
  this->is_mounted_ = false;
  ESP_LOGI(TAG, "SD/MMC card unmounted safely");

  // Re-register now that the drain above is done: registered-but-unmounted is the normal state
  // for this device (see setup()'s comment) — unregistering here was only ever about the
  // teardown window itself, not about removing the device from the registry permanently.
  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
    }

  return storage::StorageError::OK;
}

bool SdMmc::update_card_info() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return false;

  this->total_bytes_ = (uint64_t) this->card_->csd.capacity * this->card_->csd.sector_size;

  FATFS *fs;
  DWORD fre_clust;
  char path_buf[8];
  snprintf(path_buf, sizeof(path_buf), "%s/", this->mount_path_);
  if (f_getfree(path_buf, &fre_clust, &fs) == FR_OK) {
    uint64_t total = (uint64_t) ((fs->n_fatent - 2) * fs->csize) * fs->ssize;
    uint64_t free_b = (uint64_t) (fre_clust * fs->csize) * fs->ssize;
    this->used_bytes_ = total - free_b;
  }
  return true;
}

uint64_t SdMmc::get_free_bytes_impl() const {
  if (!this->is_mounted_)
    return 0;

  FATFS *fs;
  DWORD fre_clust;
  char path_buf[8];
  snprintf(path_buf, sizeof(path_buf), "%s/", this->mount_path_);
  if (f_getfree(path_buf, &fre_clust, &fs) != FR_OK)
    return 0;
  return (uint64_t) (fre_clust * fs->csize) * fs->ssize;
}

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SDMMC
