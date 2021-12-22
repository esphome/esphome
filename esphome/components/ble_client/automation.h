#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/ble_client.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {
class BLEClientConnectTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (event == ESP_GATTC_OPEN_EVT && param->open.status == ESP_GATT_OK)
      this->trigger();
    if (event == ESP_GATTC_SEARCH_CMPL_EVT)
      this->node_state = espbt::ClientState::ESTABLISHED;
  }
};

class BLEClientDisconnectTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (event == ESP_GATTC_DISCONNECT_EVT && memcmp(param->disconnect.remote_bda, this->parent_->remote_bda, 6) == 0)
      this->trigger();
    if (event == ESP_GATTC_SEARCH_CMPL_EVT)
      this->node_state = espbt::ClientState::ESTABLISHED;
  }
};

}  // namespace ble_client
}  // namespace esphome

#endif
