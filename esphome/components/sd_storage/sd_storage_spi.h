#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SD_STORAGE_SPI

#include "sd_storage_base.h"
#include "esphome/core/gpio.h"
#include "esphome/components/spi/spi.h"

#ifdef USE_ESP_IDF
#include "sdmmc_cmd.h"
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
#include "driver/sdspi_host.h"
#endif
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#endif

namespace esphome::sd_storage {

static const char *const TAG_SPI = "sd_storage.spi";

class SdSpi : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                    spi::DATA_RATE_10MHZ>,
              public SdStorageBase {
 public:
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
  // file_system option (only exists with esp32 enable_exfat): 0 auto, 1 fat32, 2 exfat.
  void set_requested_file_system(uint8_t fs) { this->requested_file_system_ = fs; }
#endif
  enum class ErrorCode : uint8_t {
    ERR_MOUNT,
    ERR_NO_CARD,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_mode_1bit(bool mode_1bit) { this->mode_1bit_ = mode_1bit; }
  void set_spi_interface(SPIInterface interface) { this->spi_interface_ = interface; }
  void set_data1_pin(GPIOPin *pin) { this->data1_pin_ = pin; }
  void set_data2_pin(GPIOPin *pin) { this->data2_pin_ = pin; }

  // Unlike SdMmc (dedicated SDIO controller, always task-safe), an SD card in SPI mode sits on
  // a general SPI bus that other main-loop-driven devices may share, so it is NOT task-safe by
  // default. A user who KNOWS this card is alone on its bus can opt in with assume_exclusive_bus
  // (config key + FINAL_VALIDATE contract). Only then -- and only where the background worker
  // task actually exists -- do we advertise STORAGE_CAP_IO_TASK_SAFE.
  void set_assume_exclusive_bus(bool assume) { this->assume_exclusive_bus_ = assume; }
  uint8_t get_capabilities() const override {
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
    if (this->assume_exclusive_bus_)
      return storage::StorageCaps::STORAGE_CAP_IO_TASK_SAFE;
#endif
    return 0;
  }

  storage::StorageError mount() override;
  storage::StorageError unmount() override;

 protected:
  SdFileHandle *get_handle_pool() override { return this->handle_pool_; }
  uint64_t get_free_bytes_impl() const override;
  uint32_t get_block_size_impl() const override;

  bool update_card_info();

  static const char *error_code_to_str(ErrorCode code);

  bool mode_1bit_{true};
  SPIInterface spi_interface_{};
  GPIOPin *data1_pin_{nullptr};
  GPIOPin *data2_pin_{nullptr};
  bool assume_exclusive_bus_{false};  // opt-in: card is alone on its SPI bus -> task-safe I/O
  ErrorCode init_error_{ErrorCode::ERR_MOUNT};

#ifdef USE_ESP_IDF
  sdmmc_card_t *card_{nullptr};
#ifdef USE_STORAGE_FILE_SYSTEM_SELECT
  uint8_t requested_file_system_{0};
  sdspi_dev_handle_t sdspi_handle_{-1};
  // Manual mirror of esp_vfs_fat_sdspi_mount -- same public-API steps, with the probe
  // window between diskio registration and f_mount (see SdMmc::mount_manual_).
  esp_err_t mount_manual_(sdmmc_host_t &host, sdspi_device_config_t &slot_config, uint32_t max_freq_khz);
  void unmount_manual_();
#endif
#endif

  SdFileHandle handle_pool_[MAX_OPEN_FILES]{};
};

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SPI
