#pragma once

#include <cinttypes>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_pro_check {

enum SensorType {
  STANDARD_BOTTOM_UP = 0x03,
  TOP_DOWN_AIR_ABOVE = 0x04,
  BOTTOM_UP_WATER = 0x05,
  LIPPERT_BOTTOM_UP = 0x06,
  PLUS_BOTTOM_UP = 0x08,
  PRO_UNIVERSAL = 0xC  // Pro Check Universal

  // all other values are reserved
};

// Sensor read quality.  If sensor is poorly placed or tank level
// gets too low the read quality will show and the distanace
// measurement may be inaccurate.
enum SensorReadQuality { QUALITY_HIGH = 0x3, QUALITY_MED = 0x2, QUALITY_LOW = 0x1, QUALITY_NONE = 0x0 };

class MopekaProCheck : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_level(sensor::Sensor *level) { level_ = level; };
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; };
  void set_battery_level(sensor::Sensor *bat) { battery_level_ = bat; };
  void set_distance(sensor::Sensor *distance) { distance_ = distance; };
  void set_tank_full(float full) { full_mm_ = full; };
  void set_tank_empty(float empty) { empty_mm_ = empty; };

 protected:
  uint64_t address_;
  sensor::Sensor *level_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *distance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};

  uint32_t full_mm_;
  uint32_t empty_mm_;

  uint8_t parse_battery_level_(const std::vector<uint8_t> &message);
  uint32_t parse_distance_(const std::vector<uint8_t> &message);
  uint8_t parse_temperature_(const std::vector<uint8_t> &message);
  SensorReadQuality parse_read_quality_(const std::vector<uint8_t> &message);
};

}  // namespace mopeka_pro_check
}  // namespace esphome

#endif
