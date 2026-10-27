#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::xiaomi_hhccjcy10 {

class XiaomiHHCCJCY10 final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void dump_config() override;
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

}  // namespace esphome::xiaomi_hhccjcy10
