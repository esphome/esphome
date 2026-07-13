#include "sd_storage_spi.h"

#ifdef USE_SD_STORAGE_SPI

#include "esphome/core/log.h"
#include <cinttypes>
#include <cstdio>

extern "C" {
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
}
#include "ff.h"
#include "diskio_sdmmc.h"
#include "esphome/components/storage/storage.h"

#ifndef VFS_FAT_MOUNT_DEFAULT_CONFIG
#define VFS_FAT_MOUNT_DEFAULT_CONFIG() \
  { .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 0, .disk_status_check_enable = false, }
#endif

namespace esphome::sd_storage {

using storage::FileHandle;
using storage::FileStat;
using storage::OpenMode;
using storage::StorageError;
using storage::StorageInfo;

void SdSpi::setup() {
  ESP_LOGI(TAG_SPI, "Initializing SD card in SPI mode");

  auto setup_input_pullup = [](GPIOPin *pin) {
    pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    pin->setup();
  };
  if (this->data1_pin_ != nullptr)
    setup_input_pullup(this->data1_pin_);
  if (this->data2_pin_ != nullptr)
    setup_input_pullup(this->data2_pin_);
  if (this->cd_pin_ != nullptr)
    setup_input_pullup(this->cd_pin_);

  this->spi_setup();

  // Register before attempting to mount, not only on success — get_info() reports is_mounted
  // correctly either way, and this lets the device (and its mount/unmount/list_files actions)
  // show up in the registry even if the initial mount fails, instead of only existing once a
  // card happens to be present. No mark_failed() here: a failed mount is not a broken
  // component, and this lets sd_storage.mount retry later without a reboot.
  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG_SPI, "Storage registration failed");
      this->mark_failed();
    }

  if (this->cd_pin_ != nullptr) {
    // With a CD pin configured, only mount if a card is actually seen at boot — otherwise wait
    // for loop()'s polling to pick up an insertion later, rather than logging a spurious "Failed
    // to mount" for a socket that's simply empty right now. No mark_failed() either way, same
    // rationale as above.
    if (this->card_present_()) {
      if (this->mount() != StorageError::OK) {
        ESP_LOGE(TAG_SPI, "Failed to mount SD card");
      }
    } else {
      ESP_LOGI(TAG_SPI, "Waiting for card (CD)");
    }
  } else if (this->mount() != StorageError::OK) {
    ESP_LOGE(TAG_SPI, "Failed to mount SD card");
  }
}

void SdSpi::loop() { this->loop_cd_(); }

void SdSpi::dump_config() {
  ESP_LOGCONFIG(TAG_SPI, "SD Storage (SPI):");
  ESP_LOGCONFIG(TAG_SPI, "  Mounted: %s", this->is_mounted_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG_SPI, "  Mount path: %s", this->mount_path_);
  ESP_LOGCONFIG(TAG_SPI, "  Mode 1 bit: %s", YESNO(this->mode_1bit_));
  ESP_LOGCONFIG(TAG_SPI, "  CS Pin: %d", spi::Utility::get_pin_no(this->cs_));
  log_pin(TAG_SPI, "  CD Pin: ", this->cd_pin_);
  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG_SPI, "  Card Type: %s", SdStorageBase::card_type_to_string(this->card_type_));
    ESP_LOGCONFIG(TAG_SPI, "  Total bytes: %" PRIu64, this->total_bytes_);
    ESP_LOGCONFIG(TAG_SPI, "  Used bytes: %" PRIu64, this->used_bytes_);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG_SPI, "Setup failed: %s", SdSpi::error_code_to_str(this->init_error_));
  }
}

const char *SdSpi::error_code_to_str(ErrorCode code) {
  switch (code) {
    case ErrorCode::ERR_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERR_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

StorageError SdSpi::mount() {
  ESP_LOGI(TAG_SPI, "Mounting SD card via SPI");

  esp_vfs_fat_sdmmc_mount_config_t mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG();
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 16;
  mount_config.allocation_unit_size = 256 * 1024;

  const auto init_err = sdspi_host_init();
  if (init_err != ESP_OK) {
    ESP_LOGE(TAG_SPI, "Failed to init sdspi host: %s", esp_err_to_name(init_err));
    this->init_error_ = ErrorCode::ERR_MOUNT;
    return StorageError::NOT_READY;
  }

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.host_id = this->spi_interface_;
  slot_config.gpio_cs = static_cast<gpio_num_t>(spi::Utility::get_pin_no(this->cs_));

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = this->spi_interface_;

  esp_err_t mount_error = ESP_OK;
  for (const auto freq_khz : {SDMMC_FREQ_DEFAULT, SDMMC_FREQ_PROBING}) {
    host.max_freq_khz = freq_khz;
    mount_error = esp_vfs_fat_sdspi_mount(this->mount_path_, &host, &slot_config, &mount_config, &this->card_);
    if (mount_error != ESP_ERR_INVALID_RESPONSE)
      break;
  }

  if (mount_error != ESP_OK) {
    ESP_LOGE(TAG_SPI, "Failed to mount FAT fs: %s", esp_err_to_name(mount_error));
    this->init_error_ =
        (mount_error == ESP_FAIL || mount_error == ESP_ERR_INVALID_CRC) ? ErrorCode::ERR_MOUNT : ErrorCode::ERR_NO_CARD;
    return StorageError::NOT_READY;
  }

  if (this->card_->is_mmc) {
    this->card_type_ = CardType::MMC;
  } else if (this->card_->is_sdio) {
    this->card_type_ = CardType::SDIO;
  } else {
    this->card_type_ = (this->card_->ocr & SD_OCR_SDHC_CAP) ? CardType::SDHC : CardType::SDSC;
  }

  this->is_mounted_ = true;
  this->set_fatfs_drive_(ff_diskio_get_pdrv_card(this->card_));
  this->update_card_info();

  ESP_LOGI(TAG_SPI, "SD card mounted at %s (max %" PRIu32 " kHz, real %" PRIu32 " kHz)", this->mount_path_,
           static_cast<uint32_t>(this->card_->max_freq_khz), static_cast<uint32_t>(this->card_->real_freq_khz));

  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG_SPI, "Storage registration failed");
      this->mark_failed();
    }

  this->on_mounted_.call(this->mount_path_);

  return StorageError::OK;
}

StorageError SdSpi::unmount() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return StorageError::OK;

  // Unregister before the VFS unmount below — the registry contract guarantees
  // unregister_storage() doesn't return until any in-flight storage_worker data-plane calls
  // against this device have drained (closing any handles the worker itself opened), so it's
  // safe to tear the filesystem down immediately afterward.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->unregister_storage(this);

  ESP_LOGI(TAG_SPI, "Syncing filesystem before unmount");
  // Closes any handles still open from user/lambda code, while the VFS is still mounted to
  // receive the flush/close calls.
  this->flush_open_handles_();
  ESP_LOGI(TAG_SPI, "All data flushed");

  esp_vfs_fat_sdcard_unmount(this->mount_path_, this->card_);
  this->card_ = nullptr;
  this->is_mounted_ = false;
  sdspi_host_deinit();
  ESP_LOGI(TAG_SPI, "SD card unmounted safely");

  // Re-register now that the drain above is done: registered-but-unmounted is the normal state
  // for this device (see setup()'s comment) — unregistering here was only ever about the
  // teardown window itself, not about removing the device from the registry permanently.
  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal — do not run with a silently missing device.
      ESP_LOGE(TAG_SPI, "Storage registration failed");
      this->mark_failed();
    }

  return StorageError::OK;
}

bool SdSpi::update_card_info() {
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

uint64_t SdSpi::get_free_bytes_impl() const {
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

uint32_t SdSpi::get_block_size_impl() const { return (this->card_ != nullptr) ? this->card_->csd.sector_size : 512; }

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SPI
