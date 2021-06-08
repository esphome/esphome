#pragma once

#include "ble_descriptor.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

class BLE2901 : public BLEDescriptor {
 public:
  BLE2901(const std::string value);
  BLE2901(uint8_t *data, size_t length);
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
