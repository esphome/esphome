#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SD_STORAGE_SPI

#include "sd_storage_base.h"
#include "esphome/core/gpio.h"
#include "esphome/components/spi/spi.h"

#ifdef USE_ESP_IDF
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#endif

namespace esphome::sd_storage {

static const char *const TAG_SPI = "sd_storage.spi";

class SdSpi : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                    spi::DATA_RATE_10MHZ>,
              public SdStorageBase {
 public:
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
  ErrorCode init_error_{ErrorCode::ERR_MOUNT};

#ifdef USE_ESP_IDF
  sdmmc_card_t *card_{nullptr};
#endif

  SdFileHandle handle_pool_[MAX_OPEN_FILES]{};
};

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SPI
