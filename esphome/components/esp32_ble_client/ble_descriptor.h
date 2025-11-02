#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_DEVICE

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

#endif  // USE_ESP32_BLE_DEVICE
#endif  // USE_ESP32
