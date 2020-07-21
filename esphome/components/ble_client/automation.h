#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/ble_client.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_client {
class BLEClientConnectTrigger : public Trigger<const BLEClient &>, public BLEClientNode {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
      case ESP_GATTC_OPEN_EVT: {
        if (param->open.status == ESP_GATT_OK) {
          this->trigger(*this->parent_);
        }
        break;
      }
      default:
        break;
    }
  }
};

class BLEClientDisconnectTrigger : public Trigger<const BLEClient &>, public BLEClientNode {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
      case ESP_GATTC_DISCONNECT_EVT: {
          this->trigger(*this->parent_);
        break;
      }
      default:
        break;
    }
  }
};

}
}

#endif
