#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SD_STORAGE_SDMMC

#include "sd_storage_base.h"
#include "esphome/core/gpio.h"
#include "sdmmc_cmd.h"
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
// mount_manual_() takes sdmmc_slot_config_t by reference; the type is an anonymous-struct
// typedef and cannot be forward-declared.
#include "driver/sdmmc_host.h"
#endif

namespace esphome::sd_storage {

class SdMmc : public SdStorageBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Pin configuration
  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }
  void set_mode_1bit(bool mode_1bit) { this->mode_1bit_ = mode_1bit; }
  void set_cs_pin(GPIOPin *pin) { this->cs_pin_ = pin; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }
  // Bus speed handed to the IDF sdmmc host; codegen supplies the platform default
  // (40 MHz high speed on ESP32-P4, 20 MHz elsewhere) unless 'data_rate:' overrides it.
  void set_data_rate_khz(uint32_t data_rate_khz) { this->data_rate_khz_ = data_rate_khz; }

  storage::StorageError mount() override;
  storage::StorageError unmount() override;

  // SDMMC has a dedicated hardware controller (SDIO), not a bus shared with other
  // main-loop-driven components (unlike SdSpi, which sits on a shared SPI bus) -- safe for a
  // future async worker to drive from a background task.
  uint8_t get_capabilities() const override { return storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE; }

 protected:
  SdFileHandle *get_handle_pool() override { return this->handle_pool_; }
  storage::StorageError get_free_bytes_impl(uint64_t &free_out) const override;
  uint32_t get_block_size_impl() const override { return this->block_size_; }

  bool update_card_info();

  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{0};
  uint8_t data2_pin_{0};
  uint8_t data3_pin_{0};
  GPIOPin *cs_pin_{nullptr};
  bool mode_1bit_{false};
  uint8_t slot_{0};
  uint32_t data_rate_khz_{SDMMC_FREQ_DEFAULT};
  uint32_t block_size_{512};
  sdmmc_card_t *card_{nullptr};
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
  // Manual mirror of esp_vfs_fat_sdmmc_mount: same public-API steps, but with a probe
  // window between diskio registration and f_mount so a requested filesystem can be
  // enforced BEFORE the one mount that happens. Wrapper-based builds are untouched.
  esp_err_t mount_manual_(sdmmc_host_t &host, sdmmc_slot_config_t &slot_config);
  storage::StorageError unmount_manual_();
#endif

  SdFileHandle handle_pool_[MAX_OPEN_FILES]{};
};

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SDMMC
