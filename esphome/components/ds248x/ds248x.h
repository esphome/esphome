#pragma once

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

class DS248xSensor;

class DS248xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sleep_pin(InternalGPIOPin *pin) { sleep_pin_ = pin; }
  void set_channel_count(uint8_t count) { channel_count_ = count; }
  void set_active_pullup(bool enabled) { active_pullup_ = enabled; }
  void set_strong_pullup(bool enabled) { strong_pullup_enabled_ = enabled; }
  void set_overdrive_speed(bool enabled) { overdrive_speed_ = enabled; }

  // DS2484 Timing Parameters
  void set_val_trstl(uint8_t val) {
    ds2484_trstl_ = val;
    ds2484_mode_ = true;
  }
  void set_val_tmsp(uint8_t val) {
    ds2484_tmsp_ = val;
    ds2484_mode_ = true;
  }
  void set_val_tw0l(uint8_t val) {
    ds2484_tw0l_ = val;
    ds2484_mode_ = true;
  }
  void set_val_trec0(uint8_t val) {
    ds2484_trec0_ = val;
    ds2484_mode_ = true;
  }
  void set_val_rwpu(uint8_t val) {
    ds2484_rwpu_ = val;
    ds2484_mode_ = true;
  }

  void register_sensor(DS248xSensor *sensor);

  // --- Core API ---
  bool select_channel(uint8_t channel);
  bool ow_reset(bool &presence);
  bool ow_write_byte(uint8_t byte);
  bool ow_read_byte(uint8_t &byte);
  bool ow_write_bit(bool bit);
  bool ow_read_bit(bool &bit);

  // --- Helpers ---
  bool match_rom(uint64_t address);
  bool skip_rom();

 protected:
  InternalGPIOPin *sleep_pin_{nullptr};
  uint8_t channel_count_ = 1;
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
  int current_channel_ = -1;

  // Internal
  bool set_read_pointer_(uint8_t ptr);
  bool wait_busy_();
  bool device_reset_();
  bool device_configure_();
  bool configure_ds2484_port_(uint8_t param, uint8_t val);
  bool set_strong_pullup_mode_(bool enable);

  // Update State Machine
  void process_next_channel_(uint8_t channel_idx);
  void process_channel_readout_(uint8_t channel_idx);
  void process_sensor_readout_(uint8_t channel_idx, uint8_t sensor_idx);
};

class DS248xSensor : public sensor::Sensor {
 public:
  void set_parent(DS248xComponent *parent) { parent_ = parent; }
  void set_address(uint64_t address) { address_ = address; }
  void set_channel(uint8_t channel) { channel_ = channel; }
  void set_index(uint8_t index) { index_ = index; }  // For compatibility if needed
  void set_resolution(uint8_t resolution) { resolution_ = resolution; }
  void set_parasitic_mode(bool parasitic_mode) { parasitic_mode_ = parasitic_mode; }

  uint64_t get_address() const { return address_; }
  uint8_t get_channel() const { return channel_; }
  uint8_t get_resolution() const { return resolution_; }
  bool get_parasitic_mode() const { return parasitic_mode_; }

  bool has_resolution_update_attempted() const { return resolution_update_attempted_; }
  void set_resolution_update_attempted(bool attempted) { resolution_update_attempted_ = attempted; }

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
