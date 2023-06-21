#pragma once

#ifdef USE_ESP32

#include <utility>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/ble_client.h"

namespace esphome {
namespace ble_client {
class BLEClientConnectTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (event == ESP_GATTC_SEARCH_CMPL_EVT) {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->trigger();
    }
  }
};

class BLEClientDisconnectTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (event == ESP_GATTC_DISCONNECT_EVT &&
        memcmp(param->disconnect.remote_bda, this->parent_->get_remote_bda(), 6) == 0)
      this->trigger();
    if (event == ESP_GATTC_SEARCH_CMPL_EVT)
      this->node_state = espbt::ClientState::ESTABLISHED;
  }
};

class BLEClientPasskeyRequestTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT &&
        memcmp(param->ble_security.auth_cmpl.bd_addr, this->parent_->get_remote_bda(), 6) == 0) {
      this->trigger();
    }
  }
};

class BLEClientPasskeyNotificationTrigger : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyNotificationTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_NOTIF_EVT &&
        memcmp(param->ble_security.auth_cmpl.bd_addr, this->parent_->get_remote_bda(), 6) == 0) {
      uint32_t passkey = param->ble_security.key_notif.passkey;
      this->trigger(passkey);
    }
  }
};

class BLEClientNumericComparisonRequestTrigger : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientNumericComparisonRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_NC_REQ_EVT &&
        memcmp(param->ble_security.auth_cmpl.bd_addr, this->parent_->get_remote_bda(), 6) == 0) {
      uint32_t passkey = param->ble_security.key_notif.passkey;
      this->trigger(passkey);
    }
  }
};

class BLEWriterClientNode : public BLEClientNode {
 public:
  BLEWriterClientNode(BLEClient *ble_client) {
    ble_client->register_ble_node(this);
    ble_client_ = ble_client;
  }

  // Attempts to write the contents of value to char_uuid_.
  void write(const std::vector<uint8_t> &value);

  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 private:
  BLEClient *ble_client_;
  int ble_char_handle_ = 0;
  esp_gatt_char_prop_t char_props_;
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
};

template<typename... Ts> class BLEClientWriteAction : public Action<Ts...>, public BLEWriterClientNode {
 public:
  BLEClientWriteAction(BLEClient *ble_client) : BLEWriterClientNode(ble_client) {}

  void play(Ts... x) override {
    if (has_simple_value_) {
      return write(this->value_simple_);
    } else {
      return write(this->value_template_(x...));
    }
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

template<typename... Ts> class BLEClientPasskeyReplyAction : public Action<Ts...> {
 public:
  BLEClientPasskeyReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    uint32_t passkey;
    if (has_simple_value_) {
      passkey = this->value_simple_;
    } else {
      passkey = this->value_template_(x...);
    }
    if (passkey > 999999)
      return;
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_passkey_reply(remote_bda, true, passkey);
  }

  void set_value_template(std::function<uint32_t(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const uint32_t &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  uint32_t value_simple_{0};
  std::function<uint32_t(Ts...)> value_template_{};
};

template<typename... Ts> class BLEClientNumericComparisonReplyAction : public Action<Ts...> {
 public:
  BLEClientNumericComparisonReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    if (has_simple_value_) {
      esp_ble_confirm_reply(remote_bda, this->value_simple_);
    } else {
      esp_ble_confirm_reply(remote_bda, this->value_template_(x...));
    }
  }

  void set_value_template(std::function<bool(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const bool &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  bool value_simple_{false};
  std::function<bool(Ts...)> value_template_{};
};

template<typename... Ts> class BLEClientRemoveBondAction : public Action<Ts...> {
 public:
  BLEClientRemoveBondAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_remove_bond_device(remote_bda);
  }

 private:
  BLEClient *parent_{nullptr};
};

}  // namespace ble_client
}  // namespace esphome

#endif
