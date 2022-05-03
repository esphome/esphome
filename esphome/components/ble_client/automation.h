#pragma once

#include <utility>

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

class BLEWriterClientNode : public BLEClientNode {
 public:
  BLEWriterClientNode(BLEClient *ble_client) {
    ble_client->register_ble_node(this);
    ble_client_ = ble_client;
  }

  // Attempts to write the contents of value_ to char_uuid_.
  bool write();

  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void loop() override;

  void set_value(const std::vector<uint8_t> &value) { this->value_ = value; }

 protected:
  BLEClient *ble_client_;
  bool pending_write_ = false;

 private:
  int ble_char_handle_ = 0;
  esp_gatt_char_prop_t char_props_;
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  std::vector<uint8_t> value_;
};

template<typename... Ts> class BLEClientWriteAction : public Action<Ts...>, public BLEWriterClientNode {
 public:
  BLEClientWriteAction(BLEClient *ble_client) : BLEWriterClientNode(ble_client) {}

  void play(Ts... x) override {
    if (has_simple_value_) {
      this->set_value(this->value_simple_);
    } else {
      this->set_value(this->value_template_(x...));
    }
    if (!ble_client_->get_maintain_connection()) {
      ble_client_->set_should_connect(true);
    }
    pending_write_ = true;
  }

  void set_value_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const std::vector<uint8_t> &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

 private:
  bool has_simple_value_ = true;
  std::vector<uint8_t> value_simple_;
  std::function<std::vector<uint8_t>(Ts...)> value_template_{};
};

}  // namespace ble_client
}  // namespace esphome

#endif
