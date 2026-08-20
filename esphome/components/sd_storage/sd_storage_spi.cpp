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
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
#include "diskio_impl.h"
#include "esphome/components/storage/fatfs_select.h"
#endif
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

static const char *const TAG_SPI = "sd_storage.spi";

void SdSpi::setup() {
  ESP_LOGD(TAG_SPI, "Initializing SD card in SPI mode");

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

  // Register before attempting to mount, not only on success -- get_info() reports is_mounted
  // correctly either way, and this lets the device (and its mount/unmount/list_files actions)
  // show up in the registry even if the initial mount fails, instead of only existing once a
  // card happens to be present. No mark_failed() here: a failed mount is not a broken
  // component, and this lets sd_storage.mount retry later without a reboot.
  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal -- do not run with a silently missing device.
      ESP_LOGE(TAG_SPI, "Storage registration failed");
      this->mark_failed();
    }

  if (this->cd_pin_ != nullptr) {
    // With a CD pin configured, only mount if a card is actually seen at boot -- otherwise wait
    // for loop()'s polling to pick up an insertion later, rather than logging a spurious "Failed
    // to mount" for a socket that's simply empty right now. No mark_failed() either way, same
    // rationale as above.
    if (this->card_present_()) {
      if (this->mount() != StorageError::STORAGE_ERROR_OK) {
        ESP_LOGE(TAG_SPI, "Failed to mount SD card");
      }
    } else {
      ESP_LOGD(TAG_SPI, "Waiting for card (CD)");
    }
  } else if (this->mount() != StorageError::STORAGE_ERROR_OK) {
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
  ESP_LOGCONFIG(TAG_SPI, "  Data rate: %" PRIu32 " kHz", this->data_rate_ / 1000);
  log_pin(TAG_SPI, "  CD Pin: ", this->cd_pin_);
  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG_SPI, "  Card Type: %s", SdStorageBase::card_type_to_string(this->card_type_));
    ESP_LOGCONFIG(TAG_SPI, "  Total bytes: %" PRIu64, this->total_bytes_);
    if (this->update_card_info())
      ESP_LOGCONFIG(TAG_SPI, "  Used bytes: %" PRIu64, this->used_bytes_);
    else
      ESP_LOGCONFIG(TAG_SPI, "  Used bytes: unavailable");
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG_SPI, "Setup failed: %s", SdSpi::error_code_to_str(this->init_error_));
  }
}

const char *SdSpi::error_code_to_str(ErrorCode code) {
  switch (code) {
    case ErrorCode::ERROR_CODE_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERROR_CODE_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
esp_err_t SdSpi::mount_manual_(sdmmc_host_t &host, sdspi_device_config_t &slot_config, uint32_t max_freq_khz) {
  // Step for step what esp_vfs_fat_sdspi_mount does (all public IDF API), with the probe
  // window between diskio registration and f_mount. The frequency fallback wraps only the
  // card bring-up: everything after it is bus-speed independent.
  esp_err_t err = sdspi_host_init_device(&slot_config, &this->sdspi_handle_);
  if (err != ESP_OK)
    return err;
  host.slot = this->sdspi_handle_;
  auto *card = new sdmmc_card_t{};  // NOLINT(cppcoreguidelines-owning-memory) - freed in unmount_manual_
  err = ESP_FAIL;
  for (const uint32_t freq_khz : {max_freq_khz, static_cast<uint32_t>(SDMMC_FREQ_PROBING)}) {
    host.max_freq_khz = static_cast<int>(freq_khz);
    ESP_LOGD(TAG_SPI, "Initialising card at %" PRIu32 " kHz", freq_khz);
    err = sdmmc_card_init(&host, card);
    if (err == ESP_OK || freq_khz == static_cast<uint32_t>(SDMMC_FREQ_PROBING))
      break;
    // Same reasoning as the wrapper path: only bus-signalling failures improve at 400 kHz.
    if (err != ESP_ERR_TIMEOUT && err != ESP_ERR_INVALID_RESPONSE && err != ESP_ERR_INVALID_CRC)
      break;
    ESP_LOGW(TAG_SPI, "Card init at %" PRIu32 " kHz failed (%s), retrying at %d kHz", freq_khz, esp_err_to_name(err),
             SDMMC_FREQ_PROBING);
  }
  if (err != ESP_OK) {
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
    delete card;  // NOLINT(cppcoreguidelines-owning-memory)
    return err;
  }

  BYTE pdrv = FF_DRV_NOT_USED;
  if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == FF_DRV_NOT_USED) {
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
    delete card;  // NOLINT(cppcoreguidelines-owning-memory)
    return ESP_ERR_NO_MEM;
  }
  ff_diskio_register_sdmmc(pdrv, card);
  char drv[3] = {static_cast<char>('0' + pdrv), ':', '\0'};

  if (!storage::ensure_requested_filesystem(TAG_SPI, pdrv, drv, this->requested_file_system_,
                                            this->format_on_mismatch_)) {
    ff_diskio_register(pdrv, nullptr);
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
    delete card;  // NOLINT(cppcoreguidelines-owning-memory)
    return ESP_FAIL;
  }

  FATFS *fs = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  esp_vfs_fat_conf_t vfs_conf = {
      .base_path = this->mount_path_,
      .fat_drive = drv,
      .max_files = 16,
  };
  err = esp_vfs_fat_register_cfg(&vfs_conf, &fs);
#else
  err = esp_vfs_fat_register(this->mount_path_, drv, 16, &fs);
#endif
  if (err != ESP_OK) {
    ff_diskio_register(pdrv, nullptr);
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
    delete card;  // NOLINT(cppcoreguidelines-owning-memory)
    return err;
  }
  FRESULT res = f_mount(fs, drv, 1);
  if (res != FR_OK) {
    ESP_LOGE(TAG_SPI, "f_mount failed: %d", res);
    esp_vfs_fat_unregister_path(this->mount_path_);
    ff_diskio_register(pdrv, nullptr);
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
    delete card;  // NOLINT(cppcoreguidelines-owning-memory)
    return ESP_FAIL;
  }
  this->card_ = card;
  return ESP_OK;
}

storage::StorageError SdSpi::unmount_manual_() {
  StorageError err = StorageError::STORAGE_ERROR_OK;
  BYTE pdrv = ff_diskio_get_pdrv_card(this->card_);
  if (pdrv != FF_DRV_NOT_USED) {
    char drv[3] = {static_cast<char>('0' + pdrv), ':', '\0'};
    FRESULT res = f_mount(nullptr, drv, 0);
    if (res != FR_OK) {
      ESP_LOGW(TAG_SPI, "f_mount(unmount) failed (FRESULT %d)", res);
      err = fresult_to_storage_error(res, /*for_rmdir=*/false, /*is_write=*/false);
    }
    ff_diskio_register(pdrv, nullptr);
  } else {
    ESP_LOGW(TAG_SPI, "unmount: no diskio binding for card (pdrv lookup failed); FATFS volume not unmounted");
    err = StorageError::STORAGE_ERROR_NOT_READY;
  }
  if (esp_vfs_fat_unregister_path(this->mount_path_) != ESP_OK && err == StorageError::STORAGE_ERROR_OK)
    err = StorageError::STORAGE_ERROR_NOT_READY;
  if (this->sdspi_handle_ >= 0) {
    sdspi_host_remove_device(this->sdspi_handle_);
    this->sdspi_handle_ = -1;
  }
  delete this->card_;  // NOLINT(cppcoreguidelines-owning-memory)
  return err;
}
#endif  // USE_STORAGE_FILE_SYSTEM_SELECT

StorageError SdSpi::mount() {
  ESP_LOGD(TAG_SPI, "Mounting SD card via SPI");

  esp_vfs_fat_sdmmc_mount_config_t mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG();
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 16;
  mount_config.allocation_unit_size = 256 * 1024;

  const auto init_err = sdspi_host_init();
  if (init_err != ESP_OK) {
    ESP_LOGE(TAG_SPI, "Failed to init sdspi host: %s", esp_err_to_name(init_err));
    this->init_error_ = ErrorCode::ERROR_CODE_MOUNT;
    return StorageError::STORAGE_ERROR_NOT_READY;
  }

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.host_id = this->spi_interface_;
  slot_config.gpio_cs = static_cast<gpio_num_t>(spi::Utility::get_pin_no(this->cs_));

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = this->spi_interface_;

  // The IDF sdspi driver runs the actual transfers, not our SPIDevice delegate, so the
  // configured bus speed has to be handed over here -- otherwise 'data_rate:' is accepted by
  // the schema and silently ignored. Default comes from the SPIDevice declaration (10 MHz).
  uint32_t max_freq_khz = this->data_rate_ / 1000;
  if (max_freq_khz < SDMMC_FREQ_PROBING)
    max_freq_khz = SDMMC_FREQ_PROBING;

  esp_err_t mount_error = ESP_OK;
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
  (void) mount_config;
  mount_error = this->mount_manual_(host, slot_config, max_freq_khz);
#else
  for (const uint32_t freq_khz : {max_freq_khz, static_cast<uint32_t>(SDMMC_FREQ_PROBING)}) {
    host.max_freq_khz = static_cast<int>(freq_khz);
    ESP_LOGD(TAG_SPI, "Mounting at %" PRIu32 " kHz", freq_khz);
    mount_error = esp_vfs_fat_sdspi_mount(this->mount_path_, &host, &slot_config, &mount_config, &this->card_);
    if (mount_error == ESP_OK || freq_khz == static_cast<uint32_t>(SDMMC_FREQ_PROBING))
      break;
    // Only bus signalling failures are worth a slower retry: a card that times out, answers
    // garbage or fails CRC at speed frequently enumerates fine at the 400 kHz probing clock
    // (long traces, a bus shared with other devices, weak pull-ups). Anything else (no card,
    // no memory, bad arguments) will not improve by clocking slower.
    if (mount_error != ESP_ERR_TIMEOUT && mount_error != ESP_ERR_INVALID_RESPONSE && mount_error != ESP_ERR_INVALID_CRC)
      break;
    ESP_LOGW(TAG_SPI, "Mount at %" PRIu32 " kHz failed (%s), retrying at %d kHz", freq_khz,
             esp_err_to_name(mount_error), SDMMC_FREQ_PROBING);
  }
#endif

  if (mount_error != ESP_OK) {
    ESP_LOGE(TAG_SPI, "Failed to mount FAT fs: %s", esp_err_to_name(mount_error));
    this->init_error_ = (mount_error == ESP_FAIL || mount_error == ESP_ERR_INVALID_CRC) ? ErrorCode::ERROR_CODE_MOUNT
                                                                                        : ErrorCode::ERROR_CODE_NO_CARD;
    return StorageError::STORAGE_ERROR_NOT_READY;
  }

  if (this->card_->is_mmc) {
    this->card_type_ = CardType::CARD_TYPE_MMC;
  } else if (this->card_->is_sdio) {
    this->card_type_ = CardType::CARD_TYPE_SDIO;
  } else {
    this->card_type_ = (this->card_->ocr & SD_OCR_SDHC_CAP) ? CardType::CARD_TYPE_SDHC : CardType::CARD_TYPE_SDSC;
  }

  this->is_mounted_ = true;
#ifdef USE_STORAGE_CHANGE_FEED
  // Mount state is part of the roots listing: whoever flipped it (CD pin, hotplug, HTTP,
  // automation), the browser's change poll must see it -- including recovery after an error.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->note_dir_changed("");
#endif

  this->set_fatfs_drive_(ff_diskio_get_pdrv_card(this->card_));
  this->update_card_info();

  ESP_LOGI(TAG_SPI, "SD card mounted at %s (max %" PRIu32 " kHz, real %" PRIu32 " kHz)", this->mount_path_,
           static_cast<uint32_t>(this->card_->max_freq_khz), static_cast<uint32_t>(this->card_->real_freq_khz));

  if (storage::global_storage_registry != nullptr)
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      // Registry full = codegen/runtime device-count mismatch: the device would be invisible
      // to resolve_path()/consumers. Fatal -- do not run with a silently missing device.
      ESP_LOGE(TAG_SPI, "Storage registration failed");
      this->mark_failed();
    }

  this->on_mounted_.call(this->mount_path_);

  return StorageError::STORAGE_ERROR_OK;
}

StorageError SdSpi::unmount() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return StorageError::STORAGE_ERROR_OK;

  // Quiesce before the VFS unmount below -- same drain guarantee as unregister_storage()
  // (no in-flight storage_worker data-plane call against this device remains, handles the
  // worker opened are closed), but the device stays registered: registered-but-unmounted
  // is its normal state, so there is nothing to re-register afterwards.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->quiesce_storage(this);

  ESP_LOGD(TAG_SPI, "Syncing filesystem before unmount");
  // Closes any handles still open from user/lambda code, while the VFS is still mounted to
  // receive the flush/close calls.
  StorageError flush_err = this->flush_open_handles_();
  if (flush_err == StorageError::STORAGE_ERROR_OK)
    ESP_LOGD(TAG_SPI, "All data flushed");
  else
    ESP_LOGW(TAG_SPI, "Flush before unmount failed: %s", storage::error_to_string(flush_err));

#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
  StorageError unmount_err = this->unmount_manual_();
#else
  StorageError unmount_err = StorageError::STORAGE_ERROR_OK;
  if (esp_vfs_fat_sdcard_unmount(this->mount_path_, this->card_) != ESP_OK) {
    ESP_LOGW(TAG_SPI, "esp_vfs_fat_sdcard_unmount failed");
    unmount_err = StorageError::STORAGE_ERROR_NOT_READY;
  }
#endif
  this->card_ = nullptr;
  this->is_mounted_ = false;
#ifdef USE_STORAGE_CHANGE_FEED
  // Mount state is part of the roots listing: whoever flipped it (CD pin, hotplug, HTTP,
  // automation), the browser's change poll must see it -- including recovery after an error.
  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->note_dir_changed("");
#endif

  if (sdspi_host_deinit() != ESP_OK)
    ESP_LOGW(TAG_SPI, "sdspi_host_deinit() failed");

  // Report the flush and teardown results so a failed unmount does not look clean.
  if (flush_err != StorageError::STORAGE_ERROR_OK)
    return flush_err;
  if (unmount_err != StorageError::STORAGE_ERROR_OK)
    return unmount_err;

  ESP_LOGI(TAG_SPI, "SD card unmounted safely");
  return StorageError::STORAGE_ERROR_OK;
}

bool SdSpi::update_card_info() {
  if (!this->is_mounted_ || this->card_ == nullptr)
    return false;

  this->total_bytes_ = (uint64_t) this->card_->csd.capacity * this->card_->csd.sector_size;

  uint64_t total_bytes = 0, free_bytes = 0;
  esp_err_t err = esp_vfs_fat_info(this->mount_path_, &total_bytes, &free_bytes);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_SPI, "esp_vfs_fat_info(%s) failed: %s", this->mount_path_, esp_err_to_name(err));
    return false;
  }
  this->used_bytes_ = total_bytes - free_bytes;
  return true;
}

StorageError SdSpi::get_free_bytes_impl(uint64_t &free_out) const {
  if (!this->is_mounted_)
    return StorageError::STORAGE_ERROR_NOT_READY;

  uint64_t total_bytes = 0, free_bytes = 0;
  esp_err_t err = esp_vfs_fat_info(this->mount_path_, &total_bytes, &free_bytes);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_SPI, "esp_vfs_fat_info(%s) failed: %s", this->mount_path_, esp_err_to_name(err));
    return (err == ESP_ERR_INVALID_STATE) ? StorageError::STORAGE_ERROR_NOT_READY
                                          : StorageError::STORAGE_ERROR_READ_ERROR;
  }
  free_out = free_bytes;
  return StorageError::STORAGE_ERROR_OK;
}

uint32_t SdSpi::get_block_size_impl() const { return (this->card_ != nullptr) ? this->card_->csd.sector_size : 512; }

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SPI
