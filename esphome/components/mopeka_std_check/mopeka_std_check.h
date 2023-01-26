#pragma once

#include <vector>

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_std_check {

enum SensorType {
  STANDARD = 0x02,
  XL = 0x03,
};

class MopekaStdCheck : public Component,
                       public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_level(sensor::Sensor *level) { level_ = level; };
  void set_temperature(sensor::Sensor *temperature) {
    temperature_ = temperature;
  };
  void set_battery_level(sensor::Sensor *bat) { battery_level_ = bat; };
  void set_distance(sensor::Sensor *distance) { distance_ = distance; };
  void set_lpg_butane_ratio(float val) { lpg_butane_ratio_ = val; };
  void set_tank_full(float full) { full_mm_ = full; };
  void set_tank_empty(float empty) { empty_mm_ = empty; };

 protected:
  uint64_t address_;
  sensor::Sensor *level_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *distance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};

  float lpg_butane_ratio_;
  uint32_t full_mm_;
  uint32_t empty_mm_;

  float get_lpg_speed_of_sound(float temperature);
  uint8_t parse_battery_level_(const std::vector<uint8_t> &message);
  uint8_t parse_temperature_(const std::vector<uint8_t> &message);
};

}  // namespace mopeka_std_check
}  // namespace esphome

#endif
