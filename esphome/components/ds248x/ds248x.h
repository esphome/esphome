#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ds248x {

class DS248xTemperatureSensor;

typedef struct {
  uint8_t channel = 0;
  uint64_t address = 0;
} foundDevice_t;

class DS248xComponent : public PollingComponent, public i2c::I2CDevice {
  friend class DS248xTemperatureSensor;
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

  void register_sensor(DS248xTemperatureSensor *sensor);

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

  std::vector<foundDevice_t> found_sensors_;

  std::vector<DS248xTemperatureSensor *> sensors_;

  uint8_t read_config();
  void write_config(uint8_t cfg);

  uint8_t wait_while_busy();

  void reset_hub();
  bool reset_devices();

  void write_command(uint8_t command, uint8_t data);

  void select_channel(uint8_t channel);

  void select(uint8_t channel, uint64_t address);

  void write_to_wire(uint8_t data);

  uint8_t read_from_wire();

  bool search(uint64_t *address);
};

class DS248xTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(DS248xComponent *parent) { parent_ = parent; }
  /// Helper to get a pointer to the address as uint8_t.
  uint8_t *get_address8();
  /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

  // Set the 8-bit unsigned channel, the sensor is connected to.
  void set_channel(uint8_t channel);
  /// Set the 64-bit unsigned address for this sensor.
  void set_address(uint64_t device);
  /// Get the 64-bit unsigned address of the sensor.
  uint64_t get_address();
  /// Get the index of this sensor. (0 if using address.)
  optional<uint8_t> get_index() const;
  /// Set the index of this sensor. If using index, address will be set after setup.
  void set_index(uint8_t index);
  /// Get the set resolution for this sensor.
  uint8_t get_resolution() const;
  /// Set the resolution for this sensor.
  void set_resolution(uint8_t resolution);
  /// Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion() const;

  bool setup_sensor();
  bool read_scratch_pad();

  bool check_scratch_pad();

  float get_temp_c();

  std::string unique_id() override;

 protected:
  DS248xComponent *parent_;
  uint8_t channel_;
  uint64_t address_;
  optional<uint8_t> index_;

  uint8_t resolution_;
  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };
};

}  // namespace ds248x
}  // namespace esphome
