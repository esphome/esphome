#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"
#include "esphome/core/component.h"

namespace esphome::xiaomi_rtcgq02lm {

class XiaomiRTCGQ02LM final : public Component, public xiaomi_ble::XiaomiEncryptedDevice {
 public:
  void dump_config() override;

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
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;

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

}  // namespace esphome::xiaomi_rtcgq02lm
