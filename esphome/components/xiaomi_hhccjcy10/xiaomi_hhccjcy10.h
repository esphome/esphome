#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_hhccjcy10 {

class XiaomiHHCCJCY10 : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_moisture(sensor::Sensor *moisture) { this->moisture_ = moisture; }
  void set_conductivity(sensor::Sensor *conductivity) { this->conductivity_ = conductivity; }
  void set_illuminance(sensor::Sensor *illuminance) { this->illuminance_ = illuminance; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }

 protected:
  uint64_t address_;
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *moisture_{nullptr};
  sensor::Sensor *conductivity_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace xiaomi_hhccjcy10
}  // namespace esphome

#endif
