#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/ds248x/ds248x.h"

namespace esphome {

namespace ds248x {

class DS248xComponent;

class DS248xTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(DS248xComponent *parent) { parent_ = parent; }

  // Helper to get a pointer to the address as uint8_t.
  uint8_t *get_address8();

  // Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

  // Set the 64-bit unsigned address for this sensor.
  void set_address(uint64_t address);

  // Set the channel of the 1-Wire bus for this sensor.
  void set_channel(uint8_t channel);

  // Get the channel of 1-Wire bus for this sensor.
  uint8_t get_channel() const;

  // Get the index of this sensor. (0 if using address.)
  optional<uint8_t> get_index() const;

  // Set the index of this sensor. If using index, address will be set after setup.
  void set_index(uint8_t index);

  // Get the set resolution for this sensor.
  uint8_t get_resolution() const;

  // Set the resolution for this sensor.
  void set_resolution(uint8_t resolution);

  // Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion() const;

  bool setup_sensor();
  bool read_scratch_pad();

  bool check_scratch_pad();

  float get_temp_c();

  std::string unique_id() override;

 protected:
  DS248xComponent *parent_;
  uint64_t address_;
  optional<uint8_t> index_;

  uint8_t resolution_;
  uint8_t channel_ = 0;
  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };
};

}  // namespace ds248x

}  // namespace esphome
