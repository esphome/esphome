#pragma once

#include "ble_uuid.h"
#include "ble_characteristic.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace esp32_ble {

class BLEServer;

class BLEService {
 public:
  BLEService(ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id);
  ~BLEService();
  BLECharacteristic *get_characteristic(ESPBTUUID uuid);
  BLECharacteristic *get_characteristic(uint16_t uuid);

  BLECharacteristic *create_characteristic(const std::string uuid, esp_gatt_char_prop_t properties);
  BLECharacteristic *create_characteristic(uint16_t uuid, esp_gatt_char_prop_t properties);
  BLECharacteristic *create_characteristic(ESPBTUUID uuid, esp_gatt_char_prop_t properties);

  ESPBTUUID get_uuid() { return this->uuid_; }
  BLECharacteristic *get_last_created_characteristic() { return this->last_created_characteristic_; }
  uint16_t get_handle() { return this->handle_; }

  BLEServer *get_server() { return this->server_; }

  bool do_create(BLEServer *server);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  void start();
  void stop();

 protected:
  bool errored_{false};

  std::vector<BLECharacteristic *> characteristics_;
  BLECharacteristic *last_created_characteristic_{nullptr};
  BLEServer *server_;
  ESPBTUUID uuid_;
  uint16_t num_handles_;
  uint16_t handle_{0xFFFF};
  uint8_t inst_id_;

  SemaphoreHandle_t create_lock_;
  SemaphoreHandle_t start_lock_;
  SemaphoreHandle_t stop_lock_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
