#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include "ble_descriptor.h"

namespace esphome {
namespace esp32_ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEService;

class BLECharacteristic {
 public:
  ~BLECharacteristic();
  espbt::ESPBTUUID uuid;
  uint16_t handle;
  esp_gatt_char_prop_t properties;
  std::vector<BLEDescriptor *> descriptors;
  void parse_descriptors();
  BLEDescriptor *get_descriptor(espbt::ESPBTUUID uuid);
  BLEDescriptor *get_descriptor(uint16_t uuid);
  BLEDescriptor *get_descriptor_by_handle(uint16_t handle);
  esp_err_t write_value(uint8_t *new_val, int16_t new_val_size);
  esp_err_t write_value(uint8_t *new_val, int16_t new_val_size, esp_gatt_write_type_t write_type);
  BLEService *service;
};

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
