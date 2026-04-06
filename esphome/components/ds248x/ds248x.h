#pragma once

// DS248x I2C-to-1-Wire Bridge Family
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2482-100.pdf
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2482-800.pdf
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2484.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ds248x {

// DS248x I2C Commands
static constexpr uint8_t DS248X_COMMAND_RESET = 0xF0;
static constexpr uint8_t DS248X_COMMAND_SETREADPTR = 0xE1;
static constexpr uint8_t DS248X_COMMAND_WRITECONFIG = 0xD2;
static constexpr uint8_t DS248X_COMMAND_CHANNELSELECT = 0xC3;
static constexpr uint8_t DS248X_COMMAND_RESETWIRE = 0xB4;
static constexpr uint8_t DS248X_COMMAND_WRITEBYTE = 0xA5;
static constexpr uint8_t DS248X_COMMAND_READBYTE = 0x96;
static constexpr uint8_t DS248X_COMMAND_TRIPLET = 0x78;

// DS248x Status Register Bits
static constexpr uint8_t DS248X_STATUS_BUSY = 0x01;
static constexpr uint8_t DS248X_STATUS_PPD = 0x02;
static constexpr uint8_t DS248X_STATUS_SD = 0x04;
static constexpr uint8_t DS248X_STATUS_RST = 0x10;
static constexpr uint8_t DS248X_STATUS_SBR = 0x20;
static constexpr uint8_t DS248X_STATUS_TSB = 0x40;
static constexpr uint8_t DS248X_STATUS_DIR = 0x80;

// DS248x Register Pointers
static constexpr uint8_t DS248X_POINTER_STATUS = 0xF0;
static constexpr uint8_t DS248X_POINTER_DATA = 0xE1;
static constexpr uint8_t DS248X_POINTER_CONFIG = 0xC3;

// DS248x Configuration Bits
static constexpr uint8_t DS248X_CONFIG_ACTIVE_PULLUP = 0x01;
static constexpr uint8_t DS248X_CONFIG_STRONG_PULLUP = 0x04;
static constexpr uint8_t DS248X_CONFIG_OVERDRIVE = 0x08;

/**
 * @brief DS248x I2C-to-1-Wire Bridge Component.
 *
 * This component manages the DS248x chip (DS2482-100, DS2482-800, DS2484).
 * It provides low-level 1-Wire bus operations via I2C.
 *
 * Usage: Configure DS248xOneWireBus instances for each channel.
 * These buses implement the one_wire::OneWireBus interface for compatibility
 * with all existing 1-Wire device components (dallas_temp, etc.).
 */
class DS248xComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_sleep_pin(InternalGPIOPin *pin) { this->sleep_pin_ = pin; }
  void set_bus_sleep(bool enabled) { this->bus_sleep_ = enabled; }
  void set_hub_sleep(bool enabled) { this->hub_sleep_ = enabled; }
  void set_channel_count(uint8_t count) { this->channel_count_ = count; }
  void set_active_pullup(bool enabled) { this->active_pullup_ = enabled; }
  void set_strong_pullup(bool enabled) { this->strong_pullup_enabled_ = enabled; }
  void set_overdrive_speed(bool enabled) { this->overdrive_speed_ = enabled; }

  // DS2484 Timing Parameters
  void set_val_trstl(uint8_t val) {
    this->ds2484_trstl_ = val;
    this->ds2484_mode_ = true;
  }
  void set_val_tmsp(uint8_t val) {
    this->ds2484_tmsp_ = val;
    this->ds2484_mode_ = true;
  }
  void set_val_tw0l(uint8_t val) {
    this->ds2484_tw0l_ = val;
    this->ds2484_mode_ = true;
  }
  void set_val_trec0(uint8_t val) {
    this->ds2484_trec0_ = val;
    this->ds2484_mode_ = true;
  }
  void set_val_rwpu(uint8_t val) {
    this->ds2484_rwpu_ = val;
    this->ds2484_mode_ = true;
  }

  /// Get the channel count (1 for DS2482-100/DS2484, 8 for DS2482-800)
  uint8_t get_channel_count() const { return this->channel_count_; }

  // --- Core 1-Wire API (used by DS248xOneWireBus) ---
  bool select_channel(uint8_t channel);
  bool ow_reset(bool &presence);
  bool ow_write_byte(uint8_t byte, bool keep_strong_pullup = false);
  bool ow_read_byte(uint8_t &byte);

  // --- Search support (used by DS248xOneWireBus) ---
  uint8_t search_triplet(bool search_direction);

 protected:
  InternalGPIOPin *sleep_pin_{nullptr};
  uint8_t channel_count_ = 1;
  bool bus_sleep_{false};
  bool hub_sleep_{false};
  bool active_pullup_ = false;
  bool strong_pullup_enabled_ = false;
  bool overdrive_speed_ = false;

  // DS2484 Config
  bool ds2484_mode_ = false;
  uint8_t ds2484_trstl_ = 0;
  uint8_t ds2484_tmsp_ = 0;
  uint8_t ds2484_tw0l_ = 0;
  uint8_t ds2484_trec0_ = 0;
  uint8_t ds2484_rwpu_ = 0;

  int8_t current_channel_{-1};
  bool strong_pullup_active_{false};
  uint8_t last_config_byte_{0xFF};

  // Internal helpers
  bool set_read_pointer_(uint8_t ptr);
  bool wait_busy_();
  bool device_reset_();
  bool device_configure_();
  bool configure_ds2484_port_(uint8_t param, uint8_t val);
  bool set_strong_pullup_mode_(bool enable);
};

}  // namespace ds248x
}  // namespace esphome
