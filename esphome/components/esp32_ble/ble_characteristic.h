#pragma once

#include "ble_uuid.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace esp32_ble {

class BLEService;

class BLECharacteristic {
 public:
  BLECharacteristic(const ESPBTUUID uuid, esp_gatt_char_prop_t properties) : uuid_(uuid), properties_(properties) {
    this->set_value_lock_ = xSemaphoreCreateMutex();
  }

  void set_value(uint8_t *data, size_t length);
  void set_value(std::string value);
  void set_value(uint8_t &data);
  void set_value(uint16_t &data);
  void set_value(uint32_t &data);
  void set_value(int &data);
  void set_value(float &data);
  void set_value(double &data);

  bool do_create(BLEService *service);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  ESPBTUUID get_uuid() { return this->uuid_; }
  bool is_created() { return this->created_; }

  static const uint32_t PROPERTY_READ = 1 << 0;
  static const uint32_t PROPERTY_WRITE = 1 << 1;
  static const uint32_t PROPERTY_NOTIFY = 1 << 2;
  static const uint32_t PROPERTY_BROADCAST = 1 << 3;
  static const uint32_t PROPERTY_INDICATE = 1 << 4;
  static const uint32_t PROPERTY_WRITE_NR = 1 << 5;

 protected:
  bool created_{false};
  BLEService *service_;
  ESPBTUUID uuid_;
  esp_gatt_char_prop_t properties_;
  uint16_t handle_;
  std::string value_;
  SemaphoreHandle_t set_value_lock_;

  esp_gatt_perm_t permissions_ = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
