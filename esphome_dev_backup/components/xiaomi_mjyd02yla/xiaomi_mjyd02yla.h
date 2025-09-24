#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_mjyd02yla {

class XiaomiMJYD02YLA : public Component,
                        public binary_sensor::BinarySensorInitiallyOff,
                        public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_idle_time(sensor::Sensor *idle_time) { idle_time_ = idle_time; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
  void set_illuminance(sensor::Sensor *illuminance) { illuminance_ = illuminance; }
  void set_light(binary_sensor::BinarySensor *light) { is_light_ = light; }

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];
  sensor::Sensor *idle_time_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  binary_sensor::BinarySensor *is_light_{nullptr};
};

}  // namespace xiaomi_mjyd02yla
}  // namespace esphome

#endif
