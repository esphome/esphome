#pragma once

#include "ble_descriptor.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_server {

class BLE2902 : public BLEDescriptor {
 public:
  BLE2902();
};

}  // namespace esphome::esp32_ble_server

#endif
