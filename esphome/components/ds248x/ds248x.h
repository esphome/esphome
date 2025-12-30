#pragma once

// DS248x I2C-to-1-Wire Bridge Family
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2482-100.pdf
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2482-800.pdf
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds2484.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <vector>
#include <map>

namespace esphome {
namespace ds248x {

static const uint8_t DS248X_COMMAND_RESET = 0xF0;
static const uint8_t DS248X_COMMAND_SETREADPTR = 0xE1;
static const uint8_t DS248X_COMMAND_WRITECONFIG = 0xD2;
static const uint8_t DS248X_COMMAND_CHANNELSELECT = 0xC3;
static const uint8_t DS248X_COMMAND_RESETWIRE = 0xB4;
static const uint8_t DS248X_COMMAND_WRITEBYTE = 0xA5;
static const uint8_t DS248X_COMMAND_READBYTE = 0x96;
static const uint8_t DS248X_COMMAND_SINGLEBIT = 0x87;
static const uint8_t DS248X_COMMAND_TRIPLET = 0x78;

static const uint8_t DS248X_STATUS_BUSY = (1 << 0);
static const uint8_t DS248X_STATUS_PPD = (1 << 1);
static const uint8_t DS248X_STATUS_SD = (1 << 2);
static const uint8_t DS248X_STATUS_LL = (1 << 3);
static const uint8_t DS248X_STATUS_RST = (1 << 4);
static const uint8_t DS248X_STATUS_SBR = (1 << 5);
static const uint8_t DS248X_STATUS_TSB = (1 << 6);
static const uint8_t DS248X_STATUS_DIR = (1 << 7);

static const uint8_t DS248X_POINTER_STATUS = 0xF0;
static const uint8_t DS248X_POINTER_DATA = 0xE1;
static const uint8_t DS248X_POINTER_CHANNEL = 0xD2;
static const uint8_t DS248X_POINTER_CONFIG = 0xC3;

static const uint8_t DS248X_CONFIG_ACTIVE_PULLUP = (1 << 0);
static const uint8_t DS248X_CONFIG_STRONG_PULLUP = (1 << 2);
static const uint8_t DS248X_CONFIG_OVERDRIVE = (1 << 3);

// 1-Wire ROM Commands
static const uint8_t ONEWIRE_ROM_SEARCH = 0xF0;
static const uint8_t ONEWIRE_ROM_ALARM_SEARCH = 0xEC;
static const uint8_t ONEWIRE_ROM_READ = 0x33;
static const uint8_t ONEWIRE_ROM_MATCH = 0x55;
static const uint8_t ONEWIRE_ROM_SKIP = 0xCC;

// DS18x20 Function Commands
static const uint8_t DS18X20_CMD_CONVERT_T = 0x44;
static const uint8_t DS18X20_CMD_READ_SCRATCHPAD = 0xBE;
static const uint8_t DS18X20_CMD_WRITE_SCRATCHPAD = 0x4E;
static const uint8_t DS18X20_CMD_COPY_SCRATCHPAD = 0x48;

// DS18x20 Family Codes
static const uint8_t DS18B20_FAMILY_CODE = 0x28;
static const uint8_t DS1822_FAMILY_CODE = 0x22;
static const uint8_t DS1825_FAMILY_CODE = 0x3B;
static const uint8_t DS28EA00_FAMILY_CODE = 0x42;
static const uint8_t DS18S20_FAMILY_CODE = 0x10;

class DS248xSensor;
class DS248xOneWireBus;

/**
 * @brief DS248x I2C-to-1-Wire Bridge Component.
 *
 * This component manages the DS248x chip (DS2482-100, DS2482-800, DS2484).
 * It provides low-level 1-Wire bus operations via I2C.
 *
 * There are two usage patterns:
 * 1. Legacy mode: Using DS248xSensor directly (temperature sensors)
 * 2. OneWireBus mode: Using DS248xOneWireBus for compatibility with dallas_temp etc.
 */
class DS248xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  enum ConversionMode {
    CONVERSION_FIXED = 0,
    CONVERSION_POLL = 1,
  };

  void setup() override;
  void dump_config() override;
  void update() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sleep_pin(InternalGPIOPin *pin) { this->sleep_pin_ = pin; }
  void set_bus_sleep(bool enabled) { this->bus_sleep_ = enabled; }
  void set_hub_sleep(bool enabled) { this->hub_sleep_ = enabled; }
  void set_channel_count(uint8_t count) { this->channel_count_ = count; }
  void set_active_pullup(bool enabled) { this->active_pullup_ = enabled; }
  void set_strong_pullup(bool enabled) { this->strong_pullup_enabled_ = enabled; }
  void set_overdrive_speed(bool enabled) { this->overdrive_speed_ = enabled; }
  void set_conversion_mode(ConversionMode mode) { this->conversion_mode_ = mode; }
  void set_conversion_mode(uint8_t mode) { this->conversion_mode_ = static_cast<ConversionMode>(mode); }
  void set_alarm_search_on_boot(bool enabled) { this->alarm_search_on_boot_ = enabled; }

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

  void register_sensor(DS248xSensor *sensor);

  /// Register a OneWireBus instance (for OneWireBus mode)
  void register_bus(DS248xOneWireBus *bus);

  /// Get the channel count (1 for DS2482-100/DS2484, 8 for DS2482-800)
  uint8_t get_channel_count() const { return this->channel_count_; }

  // --- Core API ---
  bool select_channel(uint8_t channel);
  bool ow_reset(bool &presence);
  bool ow_write_byte(uint8_t byte, bool keep_strong_pullup = false);
  bool ow_read_byte(uint8_t &byte);
  bool ow_write_bit(bool bit);
  bool ow_read_bit(bool &bit);

  // --- Helpers ---
  bool match_rom(uint64_t address);
  bool skip_rom();
  ConversionMode get_conversion_mode() const { return conversion_mode_; }

  // --- Search ---
  uint8_t search_triplet(bool search_direction);
  void search();
  void alarm_search();
  void run_search_(uint8_t command, const char *label);  // NOLINT(readability-identifier-naming)

 protected:
  void process_next_channel_(uint8_t channel_idx);
  void check_conversion_status_(uint8_t channel_idx, uint32_t start_time);
  void process_channel_readout_(uint8_t channel_idx);
  void process_sensor_readout_(uint8_t channel_idx, uint8_t sensor_idx);
  void finish_update_(uint8_t next_channel_idx);
  bool recover_device_(uint8_t channel_idx, const char *reason);
  uint32_t compute_conversion_delay_ms_(uint8_t max_resolution, bool needs_spu) const;

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

  std::vector<DS248xSensor *> sensors_;
  std::vector<DS248xOneWireBus *> buses_;
  int current_channel_ = -1;
  ConversionMode conversion_mode_{CONVERSION_FIXED};
  bool recovering_{false};
  bool alarm_search_on_boot_{false};
  bool strong_pullup_active_{false};
  uint8_t last_config_byte_{0xFF};
  uint32_t busy_timeout_ms_{50};

  // Internal
  bool is_updating_ = false;
  bool set_read_pointer_(uint8_t ptr);
  bool wait_busy_();
  bool device_reset_();
  bool device_configure_();
  bool configure_ds2484_port_(uint8_t param, uint8_t val);
  bool set_strong_pullup_mode_(bool enable);
};

class DS248xSensor : public sensor::Sensor {
 public:
  void set_parent(DS248xComponent *parent) { this->parent_ = parent; }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_index(uint8_t index) { this->index_ = index; }  // For compatibility if needed
  void set_resolution(uint8_t resolution) { this->resolution_ = resolution; }
  void set_parasitic_mode(bool parasitic_mode) { this->parasitic_mode_ = parasitic_mode; }

  uint64_t get_address() const { return this->address_; }
  uint8_t get_channel() const { return this->channel_; }
  uint8_t get_resolution() const { return this->resolution_; }
  bool get_parasitic_mode() const { return this->parasitic_mode_; }

  bool has_resolution_update_attempted() const { return this->resolution_update_attempted_; }
  void set_resolution_update_attempted(bool attempted) { this->resolution_update_attempted_ = attempted; }

  std::string get_address_name();

 protected:
  DS248xComponent *parent_;
  uint64_t address_ = 0;
  uint8_t channel_ = 0;
  uint8_t index_ = 0;
  uint8_t resolution_ = 12;
  bool parasitic_mode_ = false;
  bool resolution_update_attempted_ = false;
};

}  // namespace ds248x
}  // namespace esphome
