#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ds248x {

static const uint8_t DS248X_COMMAND_RESET         = 0xF0;
static const uint8_t DS248X_COMMAND_SETREADPTR    = 0xE1;
static const uint8_t DS248X_COMMAND_WRITECONFIG   = 0xD2;
static const uint8_t DS248X_COMMAND_CHANNELSELECT = 0xC3;
static const uint8_t DS248X_COMMAND_RESETWIRE     = 0xB4;
static const uint8_t DS248X_COMMAND_WRITEBYTE     = 0xA5;
static const uint8_t DS248X_COMMAND_READBYTE      = 0x96;
static const uint8_t DS248X_COMMAND_SINGLEBIT     = 0x87;
static const uint8_t DS248X_COMMAND_TRIPLET       = 0x78;

static const uint8_t DS248X_CODE_CHANNEL0         = 0xF0;
static const uint8_t DS248X_CODE_CHANNEL1         = 0xE1;
static const uint8_t DS248X_CODE_CHANNEL2         = 0xD2;
static const uint8_t DS248X_CODE_CHANNEL3         = 0xC3;
static const uint8_t DS248X_CODE_CHANNEL4         = 0xB4;
static const uint8_t DS248X_CODE_CHANNEL5         = 0xA5;
static const uint8_t DS248X_CODE_CHANNEL6         = 0x96;
static const uint8_t DS248X_CODE_CHANNEL7         = 0x87;

static const uint8_t DS248X_POINTER_STATUS       = 0xF0;
static const uint8_t DS248X_STATUS_BUSY          = (1 << 0);
static const uint8_t DS248X_STATUS_PPD           = (1 << 1);
static const uint8_t DS248X_STATUS_SD            = (1 << 2);
static const uint8_t DS248X_STATUS_LL            = (1 << 3);
static const uint8_t DS248X_STATUS_RST           = (1 << 4);
static const uint8_t DS248X_STATUS_SBR           = (1 << 5);
static const uint8_t DS248X_STATUS_TSB           = (1 << 6);
static const uint8_t DS248X_STATUS_DIR           = (1 << 7);

static const uint8_t DS248X_POINTER_DATA         = 0xE1;

static const uint8_t DS248X_POINTER_CONFIG       = 0xC3;
static const uint8_t DS248X_CONFIG_ACTIVE_PULLUP = (1 << 0);
static const uint8_t DS248X_CONFIG_POWER_DOWN    = (1 << 1);
static const uint8_t DS248X_CONFIG_STRONG_PULLUP = (1 << 2);
static const uint8_t DS248X_CONFIG_1WIRE_SPEED   = (1 << 3);

static const uint8_t WIRE_COMMAND_SKIP           = 0xCC;
static const uint8_t WIRE_COMMAND_SELECT         = 0x55;
static const uint8_t WIRE_COMMAND_SEARCH         = 0xF0;

static const uint8_t DS248X_ERROR_TIMEOUT        = (1 << 0);
static const uint8_t DS248X_ERROR_SHORT          = (1 << 1);
static const uint8_t DS248X_ERROR_CONFIG         = (1 << 2);

static const uint8_t DALLAS_MODEL_DS18S20        = 0x10;
static const uint8_t DALLAS_MODEL_DS1822         = 0x22;
static const uint8_t DALLAS_MODEL_DS18B20        = 0x28;
static const uint8_t DALLAS_MODEL_DS1825         = 0x3B;
static const uint8_t DALLAS_MODEL_DS28EA00       = 0x42;

static const uint8_t DALLAS_COMMAND_START_CONVERSION  = 0x44;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD  = 0xBE;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;
static const uint8_t DALLAS_COMMAND_SAVE_EEPROM       = 0x48;

class DS248xSensor;

typedef struct {
  uint8_t channel = 0;
  uint64_t address = 0;
} foundDevice_t;

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
  void set_channel_count(uint8_t count) { channel_count_ = count; }

  void register_sensor(DS248xSensor *sensor);

 protected:
  uint32_t readIdx;
  uint8_t selectedChannel;
  uint64_t searchAddress;
  uint8_t searchLastDiscrepancy;
  bool last_device_found;

  InternalGPIOPin *sleep_pin_;

  bool enable_bus_sleep_ = false;
  bool enable_hub_sleep_ = false;
  bool enable_active_pullup_ = false;
  bool enable_strong_pullup_ = false;
  uint8_t channel_count_ = 1;

  std::vector<foundDevice_t> found_sensors_;

  std::vector<DS248xSensor *> sensors_;

  uint8_t read_config();
  void write_config(uint8_t cfg);

  uint8_t wait_while_busy();

  void reset_hub();
  bool reset_devices();

  void write_command(uint8_t command, uint8_t data);

  bool select_channel(uint8_t channel);

  void select(uint8_t channel, uint64_t address);

  void write_to_wire(uint8_t data);

  uint8_t read_from_wire();

  bool search(uint64_t *address);

private:
  void updateChannel(uint8_t channel);
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
  void set_address(uint64_t device);
  /// Get the 64-bit unsigned address of the sensor.
  optional<uint64_t> get_address();
  /// Get the index of this sensor. (0 if using address.)
  optional<uint8_t> get_index() const;
  /// Set the index of this sensor. If using index, address will be set after setup.
  void set_index(uint8_t index);
  /// Ignore this sensor during update. Maybe initialization failed or not found on bus.
  void ignore();
  /// should this sensor be ignored during update?
  bool isIgnored();

  virtual uint16_t millis_to_wait_for_conversion() const = 0;

  virtual bool setup_sensor() = 0;

  virtual bool update() = 0;

  bool read_scratch_pad();

  bool check_scratch_pad();

  std::string unique_id() override;

 protected:
  DS248xComponent *parent_;
  optional<uint8_t> channel_;
  optional<uint64_t> address_;
  optional<uint8_t> index_;
  bool ignored_;

  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };

  void select();
  void selectChannel();
  void write_to_wire(uint8_t data);
  bool reset_devices();
};


}  // namespace ds248x
}  // namespace esphome
