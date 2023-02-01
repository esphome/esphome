#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_rtcgq02lm {

class XiaomiRTCGQ02LM : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#ifdef USE_BINARY_SENSOR
  void set_motion(binary_sensor::BinarySensor *motion) { this->motion_ = motion; }
  void set_motion_timeout(uint16_t timeout) { this->motion_timeout_ = timeout; }

  void set_light(binary_sensor::BinarySensor *light) { this->light_ = light; }
  void set_button(binary_sensor::BinarySensor *button) { this->button_ = button; }
  void set_button_timeout(uint16_t timeout) { this->button_timeout_ = timeout; }
#endif

#ifdef USE_SENSOR
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
#endif

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];

#ifdef USE_BINARY_SENSOR
  uint16_t motion_timeout_;
  uint16_t button_timeout_;

  binary_sensor::BinarySensor *motion_{nullptr};
  binary_sensor::BinarySensor *light_{nullptr};
  binary_sensor::BinarySensor *button_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *battery_level_{nullptr};
#endif
};

}  // namespace xiaomi_rtcgq02lm
}  // namespace esphome

#endif
