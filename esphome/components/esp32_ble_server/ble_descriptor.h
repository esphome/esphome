#pragma once

#include "esphome/components/esp32_ble/ble_uuid.h"

#ifdef USE_ESP32

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble_server {

using namespace esp32_ble;

class BLECharacteristic;

class BLEDescriptor {
 public:
  BLEDescriptor(ESPBTUUID uuid, uint16_t max_len = 100);
  virtual ~BLEDescriptor();
  void do_create(BLECharacteristic *characteristic);

  void set_value(const std::string &value);
  void set_value(const uint8_t *data, size_t length);

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  bool is_created() { return this->state_ == CREATED; }
  bool is_failed() { return this->state_ == FAILED; }

 protected:
  BLECharacteristic *characteristic_{nullptr};
  ESPBTUUID uuid_;
  uint16_t handle_{0xFFFF};

  esp_attr_value_t value_;

  esp_gatt_perm_t permissions_ = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;

  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    CREATED,
  } state_{INIT};
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
