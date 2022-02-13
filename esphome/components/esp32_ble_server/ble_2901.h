#pragma once

#include "ble_descriptor.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {

class BLE2901 : public BLEDescriptor {
 public:
  BLE2901(const std::string &value);
  BLE2901(const uint8_t *data, size_t length);
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
