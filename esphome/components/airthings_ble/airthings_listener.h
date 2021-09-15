#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include <BLEDevice.h>

namespace esphome {
namespace airthings_ble {

class AirthingsListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace airthings_ble
}  // namespace esphome

#endif
