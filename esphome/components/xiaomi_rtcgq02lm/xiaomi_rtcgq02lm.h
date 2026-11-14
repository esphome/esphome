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
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(motion)
  SUB_BINARY_SENSOR(light)
  SUB_BINARY_SENSOR(button)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(battery_level)
#endif

 public:
  void dump_config() override;

#ifdef USE_BINARY_SENSOR
  void set_motion_timeout(uint16_t timeout) { this->motion_timeout_ = timeout; }

  void set_button_timeout(uint16_t timeout) { this->button_timeout_ = timeout; }
#endif

 protected:
  void publish_result(const xiaomi_ble::XiaomiParseResult &res) override;

#ifdef USE_BINARY_SENSOR
  uint16_t motion_timeout_;
  uint16_t button_timeout_;

#endif
};

}  // namespace esphome::xiaomi_rtcgq02lm
