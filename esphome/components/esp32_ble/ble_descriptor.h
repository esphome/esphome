#pragma once

#include "ble_uuid.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {

class BLECharacteristic;

class BLEDescriptor {
 public:
  BLEDescriptor(ESPBTUUID uuid, uint16_t max_len = 100);
  virtual ~BLEDescriptor();
  bool do_create(BLECharacteristic *characteristic);

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  bool is_created() { return this->created_; }

 protected:
  bool created_{false};
  BLECharacteristic *characteristic_{nullptr};
  ESPBTUUID uuid_;
  uint16_t handle_{0xFFFF};

  esp_attr_value_t value_;

  esp_gatt_perm_t permissions_ = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
