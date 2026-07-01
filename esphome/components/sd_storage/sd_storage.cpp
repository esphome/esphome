#include "sd_storage.h"

#ifdef USE_SD_STORAGE_SDMMC

#include "esphome/core/log.h"
#include <sys/stat.h>
#include <errno.h>
#include <cstdio>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ff.h"
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

static sdmmc_card_t *s_card = nullptr;

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

  if (this->mount() != storage::StorageError::OK) {
    ESP_LOGE(TAG, "Failed to mount SD/MMC card");
    this->mark_failed();
  }
}

void SdMmc::loop() {}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->is_mounted_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_);
  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %d", static_cast<uint8_t>(this->card_type_));
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
    ret = esp_vfs_fat_sdmmc_mount(this->mount_path_, &host, &slot_config, &mount_config, &s_card);
    if (ret == ESP_OK)
      break;
    ESP_LOGW(TAG, "Mount attempt %d failed: %s", attempt, esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    return storage::StorageError::NOT_READY;
  }

  if (s_card->is_mmc) {
    this->card_type_ = CardType::MMC;
  } else if (s_card->is_sdio) {
    this->card_type_ = CardType::SDIO;
  } else {
    this->card_type_ = (s_card->ocr & (1 << 30)) ? CardType::SDHC : CardType::SDSC;
  }
  this->block_size_ = s_card->csd.sector_size;
  this->is_mounted_ = true;
  this->update_card_info();

  ESP_LOGI(TAG, "SD/MMC card mounted at %s", this->mount_path_);

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->register_storage(this);

  this->on_mounted_.call(this->mount_path_);

  return storage::StorageError::OK;
}

storage::StorageError SdMmc::unmount() {
  if (!this->is_mounted_ || s_card == nullptr)
    return storage::StorageError::OK;

  if (storage::global_storage_registry != nullptr)
    storage::global_storage_registry->unregister_storage(this);

  esp_vfs_fat_sdcard_unmount(this->mount_path_, s_card);
  s_card = nullptr;
  this->is_mounted_ = false;
  ESP_LOGI(TAG, "SD/MMC card unmounted");
  return storage::StorageError::OK;
}

bool SdMmc::update_card_info() {
  if (!this->is_mounted_ || s_card == nullptr)
    return false;

  this->total_bytes_ = (uint64_t) s_card->csd.capacity * s_card->csd.sector_size;

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
