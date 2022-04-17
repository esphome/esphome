#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

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

template<typename... Ts> class BLEClientWriteAction : public Action<Ts...>, public BLEClientNode {
 public:
  BLEClientWriteAction(BLEClient *ble_client) { ble_client->register_ble_node(this); }

  void set_value(std::vector<uint8_t> value) { value_ = value; }

  void play(Ts... x) override {
    if (this->ble_char_handle_ == 0) {
      ESP_LOGW("ble_write_action", "Cannot write to BLE characteristic, ble_char_handle_ == 0");
      return;
    }
    ESP_LOGVV("ble_write_action", "Will write %d bytes: %s", this->value_.size(),
              format_hex_pretty(this->value_).c_str());
    esp_err_t err =
        esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->ble_char_handle_,
                                 value_.size(), value_.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (err != ESP_OK) {
      ESP_LOGE("ble_write_action", "Error writing to characteristic: %s!", esp_err_to_name(err));
    }
  }

  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    switch (event) {
      case ESP_GATTC_REG_EVT:
        break;
      case ESP_GATTC_OPEN_EVT:
        this->node_state = espbt::ClientState::ESTABLISHED;
        break;
      case ESP_GATTC_SEARCH_CMPL_EVT: {
        auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
        if (chr == nullptr) {
          ESP_LOGW("ble_write_action", "Characteristic %s was not found in service %s",
                   this->char_uuid_.to_string().c_str(), this->service_uuid_.to_string().c_str());
          break;
        }
        this->ble_char_handle_ = chr->handle;
        this->node_state = espbt::ClientState::ESTABLISHED;
        break;
      }
      case ESP_GATTC_DISCONNECT_EVT:
        this->node_state = espbt::ClientState::IDLE;
        this->ble_char_handle_ = 0;
        break;
      default:
        break;
    }
  }

 private:
  int ble_char_handle_ = 0;
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  std::vector<uint8_t> value_;
};

}  // namespace ble_client
}  // namespace esphome

#endif
