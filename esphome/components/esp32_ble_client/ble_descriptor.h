#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

namespace esphome::esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLECharacteristic;

class BLEDescriptor {
 public:
  espbt::ESPBTUUID uuid;
  uint16_t handle;

  BLECharacteristic *characteristic;
};

}  // namespace esphome::esp32_ble_client

#endif  // USE_ESP32
