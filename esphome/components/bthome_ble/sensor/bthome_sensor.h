#pragma once

#include "../bthome_ble.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace bthome_ble {

class BTHomeSensor : public sensor::Sensor, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_object_id(uint8_t object_id) { this->object_id_ = object_id; }
  void set_bindkey(const std::string &bindkey);
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 protected:
  uint8_t object_id_{0};
  optional<std::string> bindkey_;
};

}  // namespace bthome_ble
}  // namespace esphome

#endif
