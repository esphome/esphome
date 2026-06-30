#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SD_STORAGE_SDMMC

#include "sd_storage_base.h"
#include "esphome/core/gpio.h"

namespace esphome::sd_storage {

static const char *const TAG = "sd_storage";

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

  storage::StorageError mount() override;
  storage::StorageError unmount() override;

 protected:
  SdFileHandle *get_handle_pool() override { return this->handle_pool_; }
  uint64_t get_free_bytes_impl() const override;
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
  uint32_t block_size_{512};

  SdFileHandle handle_pool_[MAX_OPEN_FILES]{};
};

}  // namespace esphome::sd_storage

#endif  // USE_SD_STORAGE_SDMMC
