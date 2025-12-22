#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <vector>
#include <set>

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

static const uint8_t DS248X_CODE_CHANNEL0 = 0xF0;
static const uint8_t DS248X_CODE_CHANNEL1 = 0xE1;
static const uint8_t DS248X_CODE_CHANNEL2 = 0xD2;
static const uint8_t DS248X_CODE_CHANNEL3 = 0xC3;
static const uint8_t DS248X_CODE_CHANNEL4 = 0xB4;
static const uint8_t DS248X_CODE_CHANNEL5 = 0xA5;
static const uint8_t DS248X_CODE_CHANNEL6 = 0x96;
static const uint8_t DS248X_CODE_CHANNEL7 = 0x87;

static const uint8_t DS248X_POINTER_STATUS = 0xF0;
static const uint8_t DS248X_STATUS_BUSY = (1 << 0);
static const uint8_t DS248X_STATUS_PPD = (1 << 1);
static const uint8_t DS248X_STATUS_SD = (1 << 2);
static const uint8_t DS248X_STATUS_LL = (1 << 3);
static const uint8_t DS248X_STATUS_RST = (1 << 4);
static const uint8_t DS248X_STATUS_SBR = (1 << 5);
static const uint8_t DS248X_STATUS_TSB = (1 << 6);
static const uint8_t DS248X_STATUS_DIR = (1 << 7);

static const uint8_t DS248X_POINTER_DATA = 0xE1;

static const uint8_t DS248X_POINTER_CONFIG = 0xC3;
static const uint8_t DS248X_CONFIG_ACTIVE_PULLUP = (1 << 0);
static const uint8_t DS248X_CONFIG_POWER_DOWN = (1 << 1);
static const uint8_t DS248X_CONFIG_STRONG_PULLUP = (1 << 2);
static const uint8_t DS248X_CONFIG_1WIRE_SPEED = (1 << 3);

static const uint8_t WIRE_COMMAND_SKIP = 0xCC;
static const uint8_t WIRE_COMMAND_SELECT = 0x55;
static const uint8_t WIRE_COMMAND_SEARCH = 0xF0;

static const uint8_t DS248X_ERROR_TIMEOUT = (1 << 0);
static const uint8_t DS248X_ERROR_SHORT = (1 << 1);
static const uint8_t DS248X_ERROR_CONFIG = (1 << 2);

static const uint8_t DALLAS_MODEL_DS18S20 = 0x10;
static const uint8_t DALLAS_MODEL_DS1822 = 0x22;
static const uint8_t DALLAS_MODEL_DS18B20 = 0x28;
static const uint8_t DALLAS_MODEL_DS1825 = 0x3B;
static const uint8_t DALLAS_MODEL_DS28EA00 = 0x42;

static const uint8_t DALLAS_COMMAND_START_CONVERSION = 0x44;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD = 0xBE;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;
static const uint8_t DALLAS_COMMAND_SAVE_EEPROM = 0x48;

class DS248xSensor;

struct FoundDevice {
  uint8_t channel = 0;
  uint64_t address = 0;
};

class DS248xComponent : public PollingComponent, public i2c::I2CDevice {
  friend class DS248xSensor;

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_sleep_pin(InternalGPIOPin *pin) { sleep_pin_ = pin; }

  void set_bus_sleep(bool enabled) { enable_bus_sleep_ = enabled; }
  void set_hub_sleep(bool enabled) { enable_hub_sleep_ = enabled; }
  void set_active_pullup(bool enabled) { enable_active_pullup_ = enabled; }
  void set_strong_pullup(bool enabled) { enable_strong_pullup_ = enabled; }
  void set_overdrive_speed(bool enabled) { enable_overdrive_speed_ = enabled; }
  void set_channel_count(uint8_t count) { channel_count_ = count; }

  void set_val_trstl(uint8_t val) { val_trstl_ = val; }
  void set_val_tmsp(uint8_t val) { val_tmsp_ = val; }
  void set_val_tw0l(uint8_t val) { val_tw0l_ = val; }
  void set_val_trec0(uint8_t val) { val_trec0_ = val; }
  void set_val_rwpu(uint8_t val) { val_rwpu_ = val; }

  void register_sensor(DS248xSensor *sensor);

 protected:
  uint32_t read_idx_ = 0;
  uint8_t selected_channel_ = 0;
  uint64_t search_address_ = 0;
  uint8_t search_last_discrepancy_ = 0;
  bool last_device_found_ = false;

  InternalGPIOPin *sleep_pin_{nullptr};

  bool enable_bus_sleep_ = false;
  bool enable_hub_sleep_ = false;
  bool enable_active_pullup_ = false;
  bool enable_strong_pullup_ = false;
  bool enable_overdrive_speed_ = false;
  uint8_t channel_count_ = 1;

  optional<uint8_t> val_trstl_;
  optional<uint8_t> val_tmsp_;
  optional<uint8_t> val_tw0l_;
  optional<uint8_t> val_trec0_;
  optional<uint8_t> val_rwpu_;

  std::vector<FoundDevice> found_sensors_;

  std::vector<DS248xSensor *> sensors_;

  std::set<uint8_t> conv_cmds_;
  std::set<uint8_t>::iterator conv_cmds_iter_;

  uint8_t read_config_();
  void write_config_(uint8_t cfg);

  uint8_t is_busy_();
  uint8_t wait_while_busy_();

  void reset_hub_();
  bool reset_devices_();

  void write_command_(uint8_t command, uint8_t data);

  bool select_channel_(uint8_t channel);

  void select_(uint8_t channel, uint64_t address);

  void write_to_wire_(uint8_t data);

  uint8_t read_from_wire_();

  bool search_(uint64_t *address);

 private:
  void update_channel_(uint8_t channel);
  void start_next_conversion_();
  void update_channel_sensors_();
};

class DS248xSensor : public sensor::Sensor {
 public:
  void set_parent(DS248xComponent *parent) { parent_ = parent; }
  /// Helper to get a pointer to the address as uint8_t.
  uint8_t *get_address8();
  /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

  // Set the 8-bit unsigned channel, the sensor is connected to.
  void set_channel(uint8_t channel);
  // Get the 8-bit unsigned channel, the sensor is connected to.
  optional<uint8_t> get_channel();
  /// Set the 64-bit unsigned address for this sensor.
  void set_address(uint64_t address);
  /// Get the 64-bit unsigned address of the sensor.
  optional<uint64_t> get_address();
  /// Get the index of this sensor. (0 if using address.)
  optional<uint8_t> get_index() const;
  /// Set the index of this sensor. If using index, address will be set after setup.
  void set_index(uint8_t index);
  /// Ignore this sensor during update. Maybe initialization failed or not found on bus.
  void ignore();
  /// should this sensor be ignored during update?
  bool is_ignored();

  virtual bool setup_sensor() = 0;

  virtual bool update() = 0;

  virtual void add_conversion_commands(std::set<uint8_t> &commands) = 0;

  bool read_scratch_pad(uint8_t page = 255);

  bool check_scratch_pad();

  std::string unique_id();

 protected:
  DS248xComponent *parent_{nullptr};
  optional<uint8_t> channel_;
  optional<uint64_t> address_;
  optional<uint8_t> index_;
  bool ignored_ = false;

  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };

  void select_();
  void select_channel_();
  void write_to_wire_(uint8_t data);
  bool reset_devices_();
};

}  // namespace ds248x
}  // namespace esphome
