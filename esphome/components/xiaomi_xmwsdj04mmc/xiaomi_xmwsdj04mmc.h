#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_xmwsdj04mmc {

class XiaomiXMWSDJ04MMC final : public Component, public xiaomi_ble::XiaomiEncryptedDevice {
 public:
  void dump_config() override;
  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;

  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace esphome::xiaomi_xmwsdj04mmc
