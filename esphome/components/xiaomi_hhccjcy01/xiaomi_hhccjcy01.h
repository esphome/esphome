#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_hhccjcy01 {

class XiaomiHHCCJCY01 final : public Component, public xiaomi_ble::XiaomiDevice {
 public:
  void dump_config() override;
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_moisture(sensor::Sensor *moisture) { moisture_ = moisture; }
  void set_conductivity(sensor::Sensor *conductivity) { conductivity_ = conductivity; }
  void set_illuminance(sensor::Sensor *illuminance) { illuminance_ = illuminance; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

 protected:
  void publish_result_(const xiaomi_ble::XiaomiParseResult &res) override;

  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *moisture_{nullptr};
  sensor::Sensor *conductivity_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace esphome::xiaomi_hhccjcy01
