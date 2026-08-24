#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif // USE_BINARY_SENSOR
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif // USE_SENSOR
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_mccgq02hl {

class XiaomiMCCGQ02HL : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#ifdef USE_BINARY_SENSOR
  void set_light(binary_sensor::BinarySensor *is_light) { is_light_ = is_light; }
  void set_open(binary_sensor::BinarySensor *is_open) { is_open_ = is_open; }
#endif  // USE_BINARY_SENSOR

#ifdef USE_SENSOR
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
#endif  // USE_SENSOR

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *is_light_{nullptr};
  binary_sensor::BinarySensor *is_open_{nullptr};
#endif  // USE_BINARY_SENSOR

#ifdef USE_SENSOR
  sensor::Sensor *battery_level_{nullptr};
#endif  // USE_SENSOR
};

}  // namespace xiaomi_mccgq02hl
}  // namespace esphome

#endif