#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/ds248x/sensor/ds248x_temperature_sensor.h"

static const uint8_t NBR_CHANNELS = 8;

namespace esphome {

namespace ds248x {

enum class DS248xType : int {
  DS2482_100 = 0,
  DS2482_800 = 1,
};

class DS248xTemperatureSensor;

class DS248xComponent : public PollingComponent, public i2c::I2CDevice {
  friend class DS248xTemperatureSensor;

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_sleep_pin(InternalGPIOPin *pin) { sleep_pin_ = pin; }

  void set_ds248x_type(const DS248xType type) { ds248x_type_ = type; }
  void set_bus_sleep(bool enabled) { enable_bus_sleep_ = enabled; }
  void set_hub_sleep(bool enabled) { enable_hub_sleep_ = enabled; }
  void set_active_pullup(bool enabled) { enable_active_pullup_ = enabled; }
  void set_strong_pullup(bool enabled) { enable_strong_pullup_ = enabled; }

  void register_sensor(DS248xTemperatureSensor *sensor);

 protected:
  uint32_t read_idx_;
  uint64_t search_address_;
  uint8_t search_last_discrepancy_;
  uint8_t channel_ = 0;
  bool last_device_found_;

  InternalGPIOPin *sleep_pin_ = nullptr;

  DS248xType ds248x_type_ = DS248xType::DS2482_100;
  bool enable_bus_sleep_ = false;
  bool enable_hub_sleep_ = false;
  bool enable_active_pullup_ = false;
  bool enable_strong_pullup_ = false;

  std::vector<uint64_t> found_sensors_;
  std::vector<uint8_t> found_channel_sensors_;

  std::vector<DS248xTemperatureSensor *> channel_sensors_[NBR_CHANNELS];
  std::vector<DS248xTemperatureSensor *> sensors_;

  uint8_t read_config_();
  void write_config_(uint8_t cfg);

  uint8_t wait_while_busy_();

  void reset_hub_();
  bool set_channel_(uint8_t channel);
  uint8_t get_channel_();

  bool reset_devices_();

  void write_command_(uint8_t command, uint8_t data);

  void select_(uint64_t address);

  void write_to_wire_(uint8_t data);

  uint8_t read_from_wire_();

  bool search_(uint64_t *address);
};

}  // namespace ds248x

}  // namespace esphome
