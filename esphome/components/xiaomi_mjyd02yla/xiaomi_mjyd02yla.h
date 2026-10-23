#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

namespace esphome::xiaomi_mjyd02yla {

class XiaomiMJYD02YLA final : public Component,
                              public binary_sensor::BinarySensorInitiallyOff,
                              public xiaomi_ble::XiaomiEncryptedDevice {
 public:
  void dump_config() override;
  void set_idle_time(sensor::Sensor *idle_time) { idle_time_ = idle_time; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
  void set_illuminance(sensor::Sensor *illuminance) { illuminance_ = illuminance; }
  void set_light(binary_sensor::BinarySensor *light) { is_light_ = light; }

 protected:
  void publish_result_(const xiaomi_ble::XiaomiParseResult &res) override;

  sensor::Sensor *idle_time_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  binary_sensor::BinarySensor *is_light_{nullptr};
};

}  // namespace esphome::xiaomi_mjyd02yla
