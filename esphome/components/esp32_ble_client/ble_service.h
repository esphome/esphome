#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_DEVICE

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include "ble_characteristic.h"

#include <vector>

namespace esphome::esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEClientBase;

class BLEService {
 public:
  ~BLEService();
  bool parsed = false;
  espbt::ESPBTUUID uuid;
  uint16_t start_handle;
  uint16_t end_handle;
  std::vector<BLECharacteristic *> characteristics;
  BLEClientBase *client;
  void parse_characteristics();
  void release_characteristics();
  BLECharacteristic *get_characteristic(espbt::ESPBTUUID uuid);
  BLECharacteristic *get_characteristic(uint16_t uuid);
};

}  // namespace esphome::esp32_ble_client

#endif  // USE_ESP32_BLE_DEVICE
#endif  // USE_ESP32
