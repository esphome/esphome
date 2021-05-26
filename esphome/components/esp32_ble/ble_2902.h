#pragma once

#include "ble_descriptor.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

class BLE2902 : public BLEDescriptor {
 public:
  BLE2902();
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
