#pragma once

#include <cinttypes>
#include <vector>

#ifndef PIO_UNIT_TESTING
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#endif  // PIO_UNIT_TESTING

namespace esphome {
namespace mopeka_std_check {

enum SensorType {
  STANDARD = 0x02,
  XL = 0x03,
  ETRAILER = 0x46,
};

// 4 values in one struct so it aligns to 8 byte. One `mopeka_std_values` is 40 bit long.
struct mopeka_std_values {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint16_t time_0 : 5;
  uint16_t value_0 : 5;
  uint16_t time_1 : 5;
  uint16_t value_1 : 5;
  uint16_t time_2 : 5;
  uint16_t value_2 : 5;
  uint16_t time_3 : 5;
  uint16_t value_3 : 5;
} __attribute__((packed));

struct mopeka_std_package {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t data_0 : 8;
  uint8_t data_1 : 8;
  uint8_t raw_voltage : 8;

  uint8_t raw_temp : 6;
  bool slow_update_rate : 1;
  bool sync_pressed : 1;

  mopeka_std_values val[4];
} __attribute__((packed));

class MopekaStdCheck_Helper {
public:
  void set_propane_butane_mix(float val) { this->propane_butane_mix_ = val; };

  float get_lpg_speed_of_sound(float temperature);
  uint8_t parse_battery_level(const mopeka_std_package *message);
  int8_t parse_temperature(const mopeka_std_package *message);
protected:
  float propane_butane_mix_;
};

#ifdef USE_ESP32
class MopekaStdCheck : public MopekaStdCheck_Helper, public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_level(sensor::Sensor *level) { this->level_ = level; };
  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; };
  void set_battery_level(sensor::Sensor *bat) { this->battery_level_ = bat; };
  void set_distance(sensor::Sensor *distance) { this->distance_ = distance; };

  void set_tank_full(float full) { this->full_mm_ = full; };
  void set_tank_empty(float empty) { this->empty_mm_ = empty; };

 protected:
  uint64_t address_;
  sensor::Sensor *level_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *distance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};

  uint32_t full_mm_;
  uint32_t empty_mm_;
};
#endif  // USE_ESP32

}  // namespace mopeka_std_check
}  // namespace esphome

